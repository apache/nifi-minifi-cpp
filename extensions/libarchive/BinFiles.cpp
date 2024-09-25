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

void BinFiles::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  uint32_t val32 = 0;
  uint64_t val64 = 0;
  if (context.getProperty(MinSize, val64)) {
    this->binManager_.setMinSize(val64);
    logger_->log_debug("BinFiles: MinSize [{}]", val64);
  }
  if (context.getProperty(MaxSize, val64)) {
    this->binManager_.setMaxSize(val64);
    logger_->log_debug("BinFiles: MaxSize [{}]", val64);
  }
  if (context.getProperty(MinEntries, val32)) {
    this->binManager_.setMinEntries(val32);
    logger_->log_debug("BinFiles: MinEntries [{}]", val32);
  }
  if (context.getProperty(MaxEntries, val32)) {
    this->binManager_.setMaxEntries(val32);
    logger_->log_debug("BinFiles: MaxEntries [{}]", val32);
  }
  if (context.getProperty(MaxBinCount, maxBinCount_)) {
    logger_->log_debug("BinFiles: MaxBinCount [{}]", maxBinCount_);
  }
  if (auto max_bin_age = context.getProperty<core::TimePeriodValue>(MaxBinAge)) {
    // We need to trigger the processor even when there are no incoming flow files so that it can flush the bins.
    setTriggerWhenEmpty(true);
    this->binManager_.setBinAge(max_bin_age->getMilliseconds());
    logger_->log_debug("BinFiles: MaxBinAge [{}]", max_bin_age->getMilliseconds());
  }
  if (context.getProperty(BatchSize, batchSize_)) {
    logger_->log_debug("BinFiles: BatchSize [{}]", batchSize_);
  }
}

void BinFiles::preprocessFlowFile(const std::shared_ptr<core::FlowFile>& flow) {
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
        logger_->log_debug("BinManager move bin {} to ready bins for group {}", readyBin_.back()->getUUIDStr(), readyBin_.back()->getGroupId());
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
  logger_->log_debug("BinManager groupBinMap size {}", groupBinMap_.size());
}

void BinManager::removeOldestBin() {
  std::lock_guard < std::mutex > lock(mutex_);
  std::chrono::system_clock::time_point olddate = std::chrono::system_clock::time_point::max();
  std::unique_ptr < std::deque<std::unique_ptr<Bin>>>* oldqueue = nullptr;
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
    logger_->log_debug("BinManager move bin {} to ready bins for group {}", readyBin_.back()->getUUIDStr(), readyBin_.back()->getGroupId());
    if ((*oldqueue)->empty()) {
      groupBinMap_.erase(group);
    }
  }
  logger_->log_debug("BinManager groupBinMap size {}", groupBinMap_.size());
}

void BinManager::getReadyBin(std::deque<std::unique_ptr<Bin>> &retBins) {
  std::lock_guard < std::mutex > lock(mutex_);
  while (!readyBin_.empty()) {
    std::unique_ptr<Bin> &bin = readyBin_.front();
    retBins.push_back(std::move(bin));
    readyBin_.pop_front();
  }
}

void BinManager::addReadyBin(std::unique_ptr<Bin> ready_bin) {
  std::lock_guard<std::mutex> lock(mutex_);
  readyBin_.push_back(std::move(ready_bin));
}

bool BinManager::offer(const std::string &group, const std::shared_ptr<core::FlowFile>& flow) {
  std::lock_guard < std::mutex > lock(mutex_);
  if (flow->getSize() > maxSize_) {
    // could not be added to a bin -- too large by itself, so create a separate bin for just this guy.
    auto bin = std::make_unique<Bin>(0, ULLONG_MAX, 1, INT_MAX, "", group);
    if (!bin->offer(flow))
      return false;
    readyBin_.push_back(std::move(bin));
    logger_->log_debug("BinManager move bin {} to ready bins for group {}", readyBin_.back()->getUUIDStr(), group);
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
        logger_->log_debug("BinManager add bin {} to group {}", queue->back()->getUUIDStr(), group);
        binCount_++;
      }
    } else {
      auto bin = std::make_unique<Bin>(minSize_, maxSize_, minEntries_, maxEntries_, fileCount_, group);
      if (!bin->offer(flow))
        return false;
      queue->push_back(std::move(bin));
      binCount_++;
      logger_->log_debug("BinManager add bin {} to group {}", queue->back()->getUUIDStr(), group);
    }
  } else {
    auto queue = std::make_unique<std::deque<std::unique_ptr<Bin>>>();
    auto bin = std::make_unique<Bin>(minSize_, maxSize_, minEntries_, maxEntries_, fileCount_, group);
    if (!bin->offer(flow))
      return false;
    queue->push_back(std::move(bin));
    logger_->log_debug("BinManager add bin {} to group {}", queue->back()->getUUIDStr(), group);
    groupBinMap_.insert(std::make_pair(group, std::move(queue)));
    binCount_++;
  }

  return true;
}

bool BinFiles::resurrectFlowFiles(core::ProcessSession &session) {
  auto flow_files = file_store_.getNewFlowFiles();
  // these are already processed FlowFiles, that we own
  bool had_failure = false;
  for (auto &file : flow_files) {
    std::string group_id = getGroupId(file);
    if (!binManager_.offer(group_id, file)) {
      session.transfer(file, Failure);
      had_failure = true;
    }
    // no need to route successfully captured such files as we already own them in the Self relationship
  }
  return had_failure;
}

bool BinFiles::assumeOwnershipOfNextBatch(core::ProcessSession &session) {
  for (size_t i = 0; i < batchSize_; ++i) {
    auto flow = session.get();

    if (flow == nullptr) {
      if (i == 0) {  // Batch didn't contain a single flowfile, we should yield if there are no ready bins either
        return false;
      }
      break;
    }

    preprocessFlowFile(flow);
    std::string group_id = getGroupId(flow);

    bool offer = binManager_.offer(group_id, flow);
    if (!offer) {
      session.transfer(flow, Failure);
      continue;
    }
    session.transfer(flow, Self);
  }
  session.commit();
  return true;
}

void BinFiles::processReadyBins(std::deque<std::unique_ptr<Bin>> ready_bins, core::ProcessSession &session) {
  while (!ready_bins.empty()) {
    std::unique_ptr<Bin> bin = std::move(ready_bins.front());
    ready_bins.pop_front();

    try {
      addFlowsToSession(session, bin);
      logger_->log_debug("BinFiles start to process bin {} for group {}", bin->getUUIDStr(), bin->getGroupId());
      if (!processBin(session, bin))
        transferFlowsToFail(session, bin);
      session.commit();
    } catch(const std::exception& ex) {
      logger_->log_error("Caught Exception type: '{}' while merging ready bin: '{}'", typeid(ex).name(), ex.what());
      binManager_.addReadyBin(std::move(bin));
      session.rollback();
    }
  }
}

std::deque<std::unique_ptr<Bin>> BinFiles::gatherReadyBins(core::ProcessContext &context) {
  binManager_.gatherReadyBins();
  if (gsl::narrow<uint32_t>(binManager_.getBinCount()) > maxBinCount_) {
    // bin count reach max allowed
    context.yield();
    logger_->log_debug("BinFiles reach max bin count {}", binManager_.getBinCount());
    binManager_.removeOldestBin();
  }

  std::deque<std::unique_ptr<Bin>> ready_bins;
  binManager_.getReadyBin(ready_bins);
  return ready_bins;
}

void BinFiles::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  if (resurrectFlowFiles(session)) {
    context.yield();
    return;
  }

  const bool valid_batch = assumeOwnershipOfNextBatch(session);
  if (auto ready_bins = gatherReadyBins(context); ready_bins.empty()) {
    if (!valid_batch) {
      yield();
    }
  } else {
    processReadyBins(std::move(ready_bins), session);
  }
}

void BinFiles::transferFlowsToFail(core::ProcessSession &session, std::unique_ptr<Bin> &bin) {
  std::deque<std::shared_ptr<core::FlowFile>> &flows = bin->getFlowFile();
  for (const auto& flow : flows) {
    session.transfer(flow, Failure);
  }
  flows.clear();
}

void BinFiles::addFlowsToSession(core::ProcessSession &session, std::unique_ptr<Bin> &bin) {
  std::deque<std::shared_ptr<core::FlowFile>> &flows = bin->getFlowFile();
  for (const auto& flow : flows) {
    session.add(flow);
  }
}

void BinFiles::restore(const std::shared_ptr<core::FlowFile>& flowFile) {
  if (!flowFile) return;
  file_store_.put(flowFile);
}

std::set<core::Connectable*> BinFiles::getOutGoingConnections(const std::string &relationship) {
  auto result = core::ConnectableImpl::getOutGoingConnections(relationship);
  if (relationship == Self.getName()) {
    result.insert(this);
  }
  return result;
}

REGISTER_RESOURCE(BinFiles, Processor);

}  // namespace org::apache::nifi::minifi::processors
