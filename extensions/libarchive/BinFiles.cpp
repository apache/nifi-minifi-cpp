/**
 * @file BinFiles.cpp
 * BinFiles class implementation
 *
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
#include <stdio.h>
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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property BinFiles::MinSize(
    core::PropertyBuilder::createProperty("Minimum Group Size")
    ->withDescription("The minimum size of for the bundle")
    ->withDefaultValue<uint64_t>(0)->build());
core::Property BinFiles::MaxSize(
    core::PropertyBuilder::createProperty("Maximum Group Size")
    ->withDescription("The maximum size for the bundle. If not specified, there is no maximum.")
    ->withType(core::StandardValidators::get().UNSIGNED_LONG_VALIDATOR)->build());
core::Property BinFiles::MinEntries(
    core::PropertyBuilder::createProperty("Minimum Number of Entries")
    ->withDescription("The minimum number of files to include in a bundle")
    ->withDefaultValue<uint32_t>(1)->build());
core::Property BinFiles::MaxEntries(
    core::PropertyBuilder::createProperty("Maximum Number of Entries")
    ->withDescription("The maximum number of files to include in a bundle. If not specified, there is no maximum.")
    ->withType(core::StandardValidators::get().UNSIGNED_INT_VALIDATOR)->build());
core::Property BinFiles::MaxBinAge(
    core::PropertyBuilder::createProperty("Max Bin Age")
    ->withDescription("The maximum age of a Bin that will trigger a Bin to be complete. Expected format is <duration> <time unit>")
    ->withType(core::StandardValidators::get().TIME_PERIOD_VALIDATOR)->build());
core::Property BinFiles::MaxBinCount(
    core::PropertyBuilder::createProperty("Maximum number of Bins")
    ->withDescription("Specifies the maximum number of bins that can be held in memory at any one time")
    ->withDefaultValue<uint32_t>(100)->build());
core::Property BinFiles::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")
    ->withDescription("Maximum number of FlowFiles processed in a single session")
    ->withDefaultValue<uint32_t>(1)->build());
core::Relationship BinFiles::Original("original", "The FlowFiles that were used to create the bundle");
core::Relationship BinFiles::Failure("failure", "If the bundle cannot be created, all FlowFiles that would have been used to create the bundle will be transferred to failure");
core::Relationship BinFiles::Self("__self__", "Marks the FlowFile to be owned by this processor");
const char *BinFiles::FRAGMENT_COUNT_ATTRIBUTE = "fragment.count";
const char *BinFiles::FRAGMENT_ID_ATTRIBUTE = "fragment.identifier";
const char *BinFiles::FRAGMENT_INDEX_ATTRIBUTE = "fragment.index";
const char *BinFiles::SEGMENT_COUNT_ATTRIBUTE = "segment.count";
const char *BinFiles::SEGMENT_ID_ATTRIBUTE = "segment.identifier";
const char *BinFiles::SEGMENT_INDEX_ATTRIBUTE = "segment.index";
const char *BinFiles::SEGMENT_ORIGINAL_FILENAME = "segment.original.filename";
const char *BinFiles::TAR_PERMISSIONS_ATTRIBUTE = "tar.permissions";

void BinFiles::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(MinSize);
  properties.insert(MaxSize);
  properties.insert(MinEntries);
  properties.insert(MaxEntries);
  properties.insert(MaxBinAge);
  properties.insert(MaxBinCount);
  properties.insert(BatchSize);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Original);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

void BinFiles::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  uint32_t val32;
  uint64_t val64;
  if (context->getProperty(MinSize.getName(), val64)) {
    this->binManager_.setMinSize(val64);
    logger_->log_debug("BinFiles: MinSize [%" PRId64 "]", val64);
  }
  if (context->getProperty(MaxSize.getName(), val64)) {
    this->binManager_.setMaxSize(val64);
    logger_->log_debug("BinFiles: MaxSize [%" PRId64 "]", val64);
  }
  if (context->getProperty(MinEntries.getName(), val32)) {
    this->binManager_.setMinEntries(val32);
    logger_->log_debug("BinFiles: MinEntries [%" PRIu32 "]", val32);
  }
  if (context->getProperty(MaxEntries.getName(), val32)) {
    this->binManager_.setMaxEntries(val32);
    logger_->log_debug("BinFiles: MaxEntries [%" PRIu32 "]", val32);
  }
  if (context->getProperty(MaxBinCount.getName(), maxBinCount_)) {
    logger_->log_debug("BinFiles: MaxBinCount [%" PRIu32 "]", maxBinCount_);
  }
  std::string maxBinAgeStr;
  if (context->getProperty(MaxBinAge.getName(), maxBinAgeStr)) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(maxBinAgeStr, val64, unit) && core::Property::ConvertTimeUnitToMS(val64, unit, val64)) {
      this->binManager_.setBinAge(val64);
      logger_->log_debug("BinFiles: MaxBinAge [%" PRIu64 "]", val64);
    }
  }
  if (context->getProperty(BatchSize.getName(), batchSize_)) {
    logger_->log_debug("BinFiles: BatchSize [%" PRIu32 "]", batchSize_);
  }
}

void BinFiles::preprocessFlowFile(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/, std::shared_ptr<core::FlowFile> flow) {
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
  for (std::map<std::string, std::unique_ptr<std::deque<std::unique_ptr<Bin>>> >::iterator it=groupBinMap_.begin(); it !=groupBinMap_.end(); ++it) {
    std::unique_ptr < std::deque<std::unique_ptr<Bin>>>&queue = it->second;
    while (!queue->empty()) {
      std::unique_ptr<Bin> &bin = queue->front();
      if (bin->isReadyForMerge() || (binAge_ != ULLONG_MAX && bin->isOlderThan(binAge_))) {
        readyBin_.push_back(std::move(bin));
        queue->pop_front();
        binCount_--;
        logger_->log_debug("BinManager move bin %s to ready bins for group %s", readyBin_.back()->getUUIDStr(), readyBin_.back()->getGroupId());
      } else {
        break;
      }
    }
    if (queue->empty()) {
      emptyQueue.push_back(it->first);
    }
  }
  for (auto group : emptyQueue) {
    // erase from the map if the queue is empty for the group
    groupBinMap_.erase(group);
  }
  logger_->log_debug("BinManager groupBinMap size %d", groupBinMap_.size());
}

void BinManager::removeOldestBin() {
  std::lock_guard < std::mutex > lock(mutex_);
  uint64_t olddate = ULLONG_MAX;
  std::unique_ptr < std::deque<std::unique_ptr<Bin>>>* oldqueue;
  for (std::map<std::string, std::unique_ptr<std::deque<std::unique_ptr<Bin>>>>::iterator it=groupBinMap_.begin(); it !=groupBinMap_.end(); ++it) {
    std::unique_ptr < std::deque<std::unique_ptr<Bin>>>&queue = it->second;
    if (!queue->empty()) {
      std::unique_ptr<Bin> &bin = queue->front();
      if (bin->getBinAge() < olddate) {
        olddate = bin->getBinAge();
        oldqueue = &queue;
      }
    }
  }
  if (olddate != ULLONG_MAX) {
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

bool BinManager::offer(const std::string &group, std::shared_ptr<core::FlowFile> flow) {
  std::lock_guard < std::mutex > lock(mutex_);
  if (flow->getSize() > maxSize_) {
    // could not be added to a bin -- too large by itself, so create a separate bin for just this guy.
    std::unique_ptr<Bin> bin = std::unique_ptr < Bin > (new Bin(0, ULLONG_MAX, 1, INT_MAX, "", group));
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
        std::unique_ptr<Bin> bin = std::unique_ptr < Bin > (new Bin(minSize_, maxSize_, minEntries_, maxEntries_, fileCount_, group));
        if (!bin->offer(flow))
          return false;
        queue->push_back(std::move(bin));
        logger_->log_debug("BinManager add bin %s to group %s", queue->back()->getUUIDStr(), group);
        binCount_++;
      }
    } else {
      std::unique_ptr<Bin> bin = std::unique_ptr < Bin > (new Bin(minSize_, maxSize_, minEntries_, maxEntries_, fileCount_, group));
      if (!bin->offer(flow))
        return false;
      queue->push_back(std::move(bin));
      binCount_++;
      logger_->log_debug("BinManager add bin %s to group %s", queue->back()->getUUIDStr(), group);
    }
  } else {
    std::unique_ptr<std::deque<std::unique_ptr<Bin>>> queue = std::unique_ptr<std::deque<std::unique_ptr<Bin>>> (new std::deque<std::unique_ptr<Bin>>());
    std::unique_ptr<Bin> bin = std::unique_ptr < Bin > (new Bin(minSize_, maxSize_, minEntries_, maxEntries_, fileCount_, group));
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
  for (auto flow : flows) {
    session->transfer(flow, Failure);
  }
  flows.clear();
}

void BinFiles::addFlowsToSession(core::ProcessContext* /*context*/, core::ProcessSession *session, std::unique_ptr<Bin> &bin) {
  std::deque<std::shared_ptr<core::FlowFile>> &flows = bin->getFlowFile();
  for (auto flow : flows) {
    session->add(flow);
  }
}

void BinFiles::restore(const std::shared_ptr<core::FlowFile>& flowFile) {
  if (!flowFile) return;
  file_store_.put(flowFile);
}

void BinFiles::FlowFileStore::put(const std::shared_ptr<core::FlowFile>& flowFile) {
  {
    std::lock_guard<std::mutex> guard(flow_file_mutex_);
    incoming_files_.emplace(std::move(flowFile));
  }
  has_new_flow_file_.store(true, std::memory_order_release);
}

std::unordered_set<std::shared_ptr<core::FlowFile>> BinFiles::FlowFileStore::getNewFlowFiles() {
  bool hasNewFlowFiles = true;
  if (!has_new_flow_file_.compare_exchange_strong(hasNewFlowFiles, false, std::memory_order_acquire, std::memory_order_relaxed)) {
    return {};
  }
  std::lock_guard<std::mutex> guard(flow_file_mutex_);
  return std::move(incoming_files_);
}

std::set<std::shared_ptr<core::Connectable>> BinFiles::getOutGoingConnections(const std::string &relationship) const {
  auto result = core::Connectable::getOutGoingConnections(relationship);
  if (relationship == Self.getName()) {
    result.insert(std::static_pointer_cast<core::Connectable>(std::const_pointer_cast<core::Processor>(shared_from_this())));
  }
  return result;
}

REGISTER_RESOURCE(BinFiles, "Bins flow files into buckets based on the number of entries or size of entries");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
