/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "BinFiles.h"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <deque>
#include <utility>
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

const core::Relationship BinFiles::Self("__self__", "Marks the FlowFile to be owned by this processor");

const char *BinFiles::FRAGMENT_COUNT_ATTRIBUTE = "fragment.count";
const char *BinFiles::FRAGMENT_ID_ATTRIBUTE = "fragment.identifier";
const char *BinFiles::FRAGMENT_INDEX_ATTRIBUTE = "fragment.index";
const char *BinFiles::SEGMENT_COUNT_ATTRIBUTE = "segment.count";
const char *BinFiles::SEGMENT_ID_ATTRIBUTE = "segment.identifier";
const char *BinFiles::SEGMENT_INDEX_ATTRIBUTE = "segment.index";
const char *BinFiles::SEGMENT_ORIGINAL_FILENAME = "segment.original.filename";
const char *BinFiles::TAR_PERMISSIONS_ATTRIBUTE = "tar.permissions";

void BinFiles::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void BinFiles::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  uint32_t val32;
  uint64_t val64;
  if (context->getProperty(MinSize, val64)) {
    this->binManager_.setMinSize(val64);
    logger_->log_debug("BinFiles: MinSize [%" PRId64 "]", val64);
  }
  if (context->getProperty(MaxSize, val64)) {
    this->binManager_.setMaxSize(val64);
    logger_->log_debug("BinFiles: MaxSize [%" PRId64 "]", val64);
  }
  if (context->getProperty(MinEntries, val32)) {
    this->binManager_.setMinEntries(val32);
    logger_->log_debug("BinFiles: MinEntries [%" PRIu32 "]", val32);
  }
  if (context->getProperty(MaxEntries, val32)) {
    this->binManager_.setMaxEntries(val32);
    logger_->log_debug("BinFiles: MaxEntries [%" PRIu32 "]", val32);
  }
  if (context->getProperty(MaxBinCount, maxBinCount_)) {
    logger_->log_debug("BinFiles: MaxBinCount [%" PRIu32 "]", maxBinCount_);
  }
  if (auto max_bin_age = context->getProperty<core::TimePeriodValue>(MaxBinAge)) {
    // We need to trigger the processor even when there are no incoming flow files so that it can flush the bins.
    setTriggerWhenEmpty(true);
    this->binManager_.setBinAge(max_bin_age->getMilliseconds());
    logger_->log_debug("BinFiles: MaxBinAge [%" PRId64 "] ms", int64_t{max_bin_age->getMilliseconds().count()});
  }
  if (context->getProperty(BatchSize, batchSize_)) {
    logger_->log_debug("BinFiles: BatchSize [%" PRIu32 "]", batchSize_);
  }
}

void BinFiles::preprocessFlowFile(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/, const std::shared_ptr<core::FlowFile>& flow) {
  // handle backward compatibility with old segment attributes
  std::string value;
  if (!flow->getAttribute(BinFiles::FRAGMENT_COUNT_ATTRIBUTE, value) && flow->getAttribute(BinFiles::SEGMENT_COUNT_ATTRIBUTE, value)) {
    flow->setAttribute(BinFiles::FRAGMENT_COUNT_ATTRIBUTE, value);
  }
  if (!flow->getAttribute(BinFiles::FRAGMENT_INDEX_ATTRIBUTE, value) && flow->getAttribute(BinFiles::SEGMENT_INDEX_ATTRIBUTE, value)) {
    flow->setAttribute(BinFiles::FRAGMENT_INDEX_ATTRIBUTE, value);
  }
  if (!flow->getAttribute(BinFiles::FRAGMENT_ID_ATTRIBUTE, value) && flow->getAttribute(BinFiles::SEGMENT_ID_ATTRIBUTE, value)) {
    flow->setAttribute(BinFiles::FRAGMENT_ID_ATTRIBUTE, value);
  }
}

void BinManager::gatherReadyBins() {
  std::lock_guard < std::mutex > lock(mutex_);
  std::vector< std::string > emptyQueue;
  for (auto& [group_id, queue] : groupBinMap_) {
    while (!queue->empty()) {
      std::unique_ptr<Bin> &bin = queue->front();
      if (bin->isReadyForMerge() || (binAge_ != std::chrono::milliseconds::max() && bin->isOlderThan(binAge_))) {
        readyBin_.push_back(std::move(bin));
        queue->pop_front();
        binCount_--;
        logger_->log_debug("BinManager move bin %s to ready bins for group %s", readyBin_.back()->getUUIDStr(), readyBin_.back()->getGroupId());
      } else {
        break;
      }
    }
    if (queue->empty()) {
      emptyQueue.push_back(group_id);
    }
  }
  for (const auto& group : emptyQueue) {
    // erase from the map if the queue is empty for the group
    groupBinMap_.erase(group);
  }
  logger_->log_debug("BinManager groupBinMap size %d", groupBinMap_.size());
}

void BinManager::removeOldestBin() {
  std::lock_guard < std::mutex > lock(mutex_);
  std::chrono::system_clock::time_point olddate = std::chrono::system_clock::time_point::max();
  std::unique_ptr < std::deque<std::unique_ptr<Bin>>>* oldqueue;
  for (auto& [_, queue] : groupBinMap_) {
    if (!queue->empty()) {
      std::unique_ptr<Bin> &bin = queue->front();
      if (bin->getCreationDate() < olddate) {
        olddate = bin->getCreationDate();
        oldqueue = &queue;
      }
    }
  }
  if (olddate != std::chrono::system_clock::time_point::max()) {
    std::unique_ptr<Bin> &remove = (*oldqueue)->front();
    std::string group = remove->getGroupId();
    readyBin_.push_back(std::move(remove));
    (*oldqueue)->pop_front();
    binCount_--;
    logger_->log_debug("BinManager move bin %s to ready bins for group %s", readyBin_.back()->getUUIDStr(), readyBin_.back()->getGroupId());
    if ((*oldqueue)->empty()) {
      groupBinMap_.erase(group);
    }
  }
  logger_->log_debug("BinManager groupBinMap size %d", groupBinMap_.size());
}

void BinManager::getReadyBin(std::deque<std::unique_ptr<Bin>> &retBins) {
  std::lock_guard < std::mutex > lock(mutex_);
  while (!readyBin_.empty()) {
    std::unique_ptr<Bin> &bin = readyBin_.front();
    retBins.push_back(std::move(bin));
    readyBin_.pop_front();
  }
}

bool BinManager::offer(const std::string &group, const std::shared_ptr<core::FlowFile>& flow) {
  std::lock_guard < std::mutex > lock(mutex_);
  if (flow->getSize() > maxSize_) {
    // could not be added to a bin -- too large by itself, so create a separate bin for just this guy.
    auto bin = std::make_unique<Bin>(0, ULLONG_MAX, 1, INT_MAX, "", group);
    if (!bin->offer(flow))
      return false;
    readyBin_.push_back(std::move(bin));
    logger_->log_debug("BinManager move bin %s to ready bins for group %s", readyBin_.back()->getUUIDStr(), group);
    return true;
  }
  auto search = groupBinMap_.find(group);
  if (search != groupBinMap_.end()) {
    std::unique_ptr < std::deque<std::unique_ptr<Bin>>>&queue = search->second;
    if (!queue->empty()) {
      std::unique_ptr<Bin> &tail = queue->back();
      if (!tail->offer(flow)) {
        // last bin can not offer the flow
        auto bin = std::make_unique<Bin>(minSize_, maxSize_, minEntries_, maxEntries_, fileCount_, group);
        if (!bin->offer(flow))
          return false;
        queue->push_back(std::move(bin));
        logger_->log_debug("BinManager add bin %s to group %s", queue->back()->getUUIDStr(), group);
        binCount_++;
      }
    } else {
      auto bin = std::make_unique<Bin>(minSize_, maxSize_, minEntries_, maxEntries_, fileCount_, group);
      if (!bin->offer(flow))
        return false;
      queue->push_back(std::move(bin));
      binCount_++;
      logger_->log_debug("BinManager add bin %s to group %s", queue->back()->getUUIDStr(), group);
    }
  } else {
    auto queue = std::make_unique<std::deque<std::unique_ptr<Bin>>>();
    auto bin = std::make_unique<Bin>(minSize_, maxSize_, minEntries_, maxEntries_, fileCount_, group);
    if (!bin->offer(flow))
      return false;
    queue->push_back(std::move(bin));
    logger_->log_debug("BinManager add bin %s to group %s", queue->back()->getUUIDStr(), group);
    groupBinMap_.insert(std::make_pair(group, std::move(queue)));
    binCount_++;
  }

  return true;
}

void BinFiles::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  // Rollback is not viable for this processor!!
  {
    // process resurrected FlowFiles first
    auto flowFiles = file_store_.getNewFlowFiles();
    // these are already processed FlowFiles, that we own
    bool hadFailure = false;
    for (auto &file : flowFiles) {
      std::string groupId = getGroupId(context.get(), file);
      bool offer = this->binManager_.offer(groupId, file);
      if (!offer) {
        session->transfer(file, Failure);
        hadFailure = true;
      } else {
        // no need to route successfully captured such files as we already own them
      }
    }
    if (hadFailure) {
      context->yield();
      return;
    }
  }

  for (size_t i = 0; i < batchSize_; ++i) {
    auto flow = session->get();

    if (flow == nullptr) {
      break;
    }

    preprocessFlowFile(context.get(), session.get(), flow);
    std::string groupId = getGroupId(context.get(), flow);

    bool offer = this->binManager_.offer(groupId, flow);
    if (!offer) {
      session->transfer(flow, Failure);
      context->yield();
      return;
    }
    // assuming ownership over the incoming flowFile
    session->transfer(flow, Self);
  }

  // migrate bin to ready bin
  this->binManager_.gatherReadyBins();
  if (gsl::narrow<uint32_t>(this->binManager_.getBinCount()) > maxBinCount_) {
    // bin count reach max allowed
    context->yield();
    logger_->log_debug("BinFiles reach max bin count %d", this->binManager_.getBinCount());
    this->binManager_.removeOldestBin();
  }

  // get the ready bin
  std::deque<std::unique_ptr<Bin>> readyBins;
  binManager_.getReadyBin(readyBins);

  // process the ready bin
  while (!readyBins.empty()) {
    // create session for merge
    // we have to create a new session
    // for each merge as a rollback erases all
    // previously added files
    core::ProcessSession mergeSession(context);
    mergeSession.setMetrics(metrics_);
    std::unique_ptr<Bin> bin = std::move(readyBins.front());
    readyBins.pop_front();
    // add bin's flows to the session
    this->addFlowsToSession(context.get(), &mergeSession, bin);
    logger_->log_debug("BinFiles start to process bin %s for group %s", bin->getUUIDStr(), bin->getGroupId());
    if (!this->processBin(context.get(), &mergeSession, bin))
      this->transferFlowsToFail(context.get(), &mergeSession, bin);
    mergeSession.commit();
  }
}

void BinFiles::transferFlowsToFail(core::ProcessContext* /*context*/, core::ProcessSession *session, std::unique_ptr<Bin> &bin) {
  std::deque<std::shared_ptr<core::FlowFile>> &flows = bin->getFlowFile();
  for (const auto& flow : flows) {
    session->transfer(flow, Failure);
  }
  flows.clear();
}

void BinFiles::addFlowsToSession(core::ProcessContext* /*context*/, core::ProcessSession *session, std::unique_ptr<Bin> &bin) {
  std::deque<std::shared_ptr<core::FlowFile>> &flows = bin->getFlowFile();
  for (const auto& flow : flows) {
    session->add(flow);
  }
}

void BinFiles::restore(const std::shared_ptr<core::FlowFile>& flowFile) {
  if (!flowFile) return;
  file_store_.put(flowFile);
}

std::set<core::Connectable*> BinFiles::getOutGoingConnections(const std::string &relationship) {
  auto result = core::Connectable::getOutGoingConnections(relationship);
  if (relationship == Self.getName()) {
    result.insert(this);
  }
  return result;
}

REGISTER_RESOURCE(BinFiles, Processor);

}  // namespace org::apache::nifi::minifi::processors
