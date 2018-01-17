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
#include <queue>
#include <map>
#include <deque>
#include <utility>
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property BinFiles::MinSize("Minimum Group Size", "The minimum size of for the bundle", "0");
core::Property BinFiles::MaxSize("Maximum Group Size", "The maximum size for the bundle. If not specified, there is no maximum.", "");
core::Property BinFiles::MinEntries("Minimum Number of Entries", "The minimum number of files to include in a bundle", "1");
core::Property BinFiles::MaxEntries("Maximum Number of Entries", "The maximum number of files to include in a bundle. If not specified, there is no maximum.", "");
core::Property BinFiles::MaxBinAge("Max Bin Age", "The maximum age of a Bin that will trigger a Bin to be complete. Expected format is <duration> <time unit>", "");
core::Property BinFiles::MaxBinCount("Maximum number of Bins", "Specifies the maximum number of bins that can be held in memory at any one time", "100");
core::Relationship BinFiles::Original("original", "The FlowFiles that were used to create the bundle");
core::Relationship BinFiles::Failure("failure", "If the bundle cannot be created, all FlowFiles that would have been used to created the bundle will be transferred to failure");
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
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Original);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

void BinFiles::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  std::string value;
  int64_t valInt;
  if (context->getProperty(MinSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    this->binManager_.setMinSize(valInt);
    logger_->log_debug("BinFiles: MinSize [%d]", valInt);
  }
  value = "";
  if (context->getProperty(MaxSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    this->binManager_.setMaxSize(valInt);
    logger_->log_debug("BinFiles: MaxSize [%d]", valInt);
  }
  value = "";
  if (context->getProperty(MinEntries.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    this->binManager_.setMinEntries(valInt);
    logger_->log_debug("BinFiles: MinEntries [%d]", valInt);
  }
  value = "";
  if (context->getProperty(MaxEntries.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    this->binManager_.setMaxEntries(valInt);
    logger_->log_debug("BinFiles: MaxEntries [%d]", valInt);
  }
  value = "";
  if (context->getProperty(MaxBinCount.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    maxBinCount_ = static_cast<int> (valInt);
    logger_->log_debug("BinFiles: MaxBinCount [%d]", valInt);
  }
  value = "";
  if (context->getProperty(MaxBinAge.getName(), value) && !value.empty()) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, valInt, unit) && core::Property::ConvertTimeUnitToMS(valInt, unit, valInt)) {
      this->binManager_.setBinAge(valInt);
      logger_->log_debug("BinFiles: MaxBinAge [%d]", valInt);
    }
  }
}

void BinFiles::preprocessFlowFile(core::ProcessContext *context, core::ProcessSession *session, std::shared_ptr<core::FlowFile> flow) {
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
  std::unique_ptr < std::deque<std::unique_ptr<Bin>>>*oldqueue;
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
  std::shared_ptr<FlowFileRecord> flow = std::static_pointer_cast < FlowFileRecord > (session->get());

  if (flow != nullptr) {
    preprocessFlowFile(context.get(), session.get(), flow);
    std::string groupId = getGroupId(context.get(), flow);

    bool offer = this->binManager_.offer(groupId, flow);
    if (!offer) {
      session->transfer(flow, Failure);
      context->yield();
      return;
    }

    // remove the flowfile from the process session, it add to merge session later.
    session->remove(flow);
  }

  // migrate bin to ready bin
  this->binManager_.gatherReadyBins();
  if (this->binManager_.getBinCount() > maxBinCount_) {
    // bin count reach max allowed
    context->yield();
    logger_->log_debug("BinFiles reach max bin count %d", this->binManager_.getBinCount());
    this->binManager_.removeOldestBin();
  }

  // get the ready bin
  std::deque<std::unique_ptr<Bin>> readyBins;
  binManager_.getReadyBin(readyBins);

  // process the ready bin
  if (!readyBins.empty()) {
    // create session for merge
    core::ProcessSession mergeSession(context);
    while (!readyBins.empty()) {
      std::unique_ptr<Bin> bin = std::move(readyBins.front());
      readyBins.pop_front();
      // add bin's flows to the session
      this->addFlowsToSession(context.get(), &mergeSession, bin);
      logger_->log_debug("BinFiles start to process bin %s for group %s", bin->getUUIDStr(), bin->getGroupId());
      if (!this->processBin(context.get(), &mergeSession, bin))
          this->transferFlowsToFail(context.get(), &mergeSession, bin);
    }
    mergeSession.commit();
  }
}

void BinFiles::transferFlowsToFail(core::ProcessContext *context, core::ProcessSession *session, std::unique_ptr<Bin> &bin) {
  std::deque<std::shared_ptr<core::FlowFile>> &flows = bin->getFlowFile();
  for (auto flow : flows) {
    session->transfer(flow, Failure);
  }
  flows.clear();
}

void BinFiles::addFlowsToSession(core::ProcessContext *context, core::ProcessSession *session, std::unique_ptr<Bin> &bin) {
  std::deque<std::shared_ptr<core::FlowFile>> &flows = bin->getFlowFile();
  for (auto flow : flows) {
    session->add(flow);
  }
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
