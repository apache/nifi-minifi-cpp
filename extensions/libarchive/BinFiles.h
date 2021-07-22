/**
 * @file BinFiles.h
 * BinFiles class declaration
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
#pragma once

#include <cinttypes>
#include <limits>
#include <deque>
#include <memory>
#include <unordered_set>
#include <string>
#include <set>
#include <map>
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/gsl.h"
#include "utils/Id.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// Bin Class
class Bin {
 public:
  // Constructor
  /*!
   * Create a new Bin. Note: this object is not thread safe
   */
  explicit Bin(const uint64_t &minSize, const uint64_t &maxSize, const size_t &minEntries, const size_t & maxEntries, const std::string &fileCount, const std::string &groupId)
      : minSize_(minSize),
        maxSize_(maxSize),
        maxEntries_(maxEntries),
        minEntries_(minEntries),
        fileCount_(fileCount),
        groupId_(groupId),
        logger_(logging::LoggerFactory<Bin>::getLogger()) {
    queued_data_size_ = 0;
    creation_dated_ = utils::timeutils::getTimeMillis();
    uuid_ = utils::IdGenerator::getIdGenerator()->generate();
    logger_->log_debug("Bin %s for group %s created", getUUIDStr(), groupId_);
  }
  virtual ~Bin() {
    logger_->log_debug("Bin %s for group %s destroyed", getUUIDStr(), groupId_);
  }
  // check whether the bin is full
  bool isFull() {
    if (queued_data_size_ >= maxSize_ || queue_.size() >= maxEntries_)
      return true;
    else
      return false;
  }
  // check whether the bin meet the min required size and entries so that it can be processed for merge
  bool isReadyForMerge() {
    return isFull() || (queued_data_size_ >= minSize_ && queue_.size() >= minEntries_);
  }
  // check whether the bin is older than the time specified in msec
  bool isOlderThan(const uint64_t &duration) {
    uint64_t currentTime = utils::timeutils::getTimeMillis();
    if (currentTime > (creation_dated_ + duration))
      return true;
    else
      return false;
  }
  std::deque<std::shared_ptr<core::FlowFile>> & getFlowFile() {
    return queue_;
  }
  // offer the flowfile to the bin
  bool offer(std::shared_ptr<core::FlowFile> flow) {
    if (!fileCount_.empty()) {
      std::string value;
      if (flow->getAttribute(fileCount_, value)) {
        try {
          // for defrag case using the identification
          size_t count = std::stoul(value);
          maxEntries_ = count;
          minEntries_ = count;
        } catch (...) {
        }
      }
    }

    if ((queued_data_size_ + flow->getSize()) > maxSize_ || (queue_.size() + 1) > maxEntries_)
      return false;

    queue_.push_back(flow);
    queued_data_size_ += flow->getSize();
    logger_->log_debug("Bin %s for group %s offer size %zu byte %" PRIu64 " min_entry %zu max_entry %zu", getUUIDStr(), groupId_, queue_.size(), queued_data_size_, minEntries_, maxEntries_);

    return true;
  }
  // getBinAge
  uint64_t getBinAge() {
    return creation_dated_;
  }
  int getSize() {
    return gsl::narrow<int>(queue_.size());
  }

  utils::SmallString<36> getUUIDStr() {
    return uuid_.to_string();
  }

  std::string getGroupId() {
    return groupId_;
  }

 private:
  uint64_t minSize_;
  uint64_t maxSize_;
  size_t maxEntries_;
  size_t minEntries_;
  // Queued data size
  uint64_t queued_data_size_;
  // Queue for the Flow File
  std::deque<std::shared_ptr<core::FlowFile>> queue_;
  uint64_t creation_dated_;
  std::string fileCount_;
  std::string groupId_;
  std::shared_ptr<logging::Logger> logger_;
  // A global unique identifier
  utils::Identifier uuid_;
};

// BinManager Class
class BinManager {
 public:
  virtual ~BinManager() {
    purge();
  }
  void setMinSize(uint64_t size) {
    minSize_ = {size};
  }
  void setMaxSize(uint64_t size) {
    maxSize_ = {size};
  }
  void setMaxEntries(uint32_t entries) {
    maxEntries_ = {entries};
  }
  void setMinEntries(uint32_t entries) {
    minEntries_ = {entries};
  }
  void setBinAge(uint64_t age) {
    binAge_ = {age};
  }
  int getBinCount() {
    return binCount_;
  }
  void setFileCount(const std::string &value) {
    fileCount_ = value;
  }
  void purge() {
    std::lock_guard<std::mutex> lock(mutex_);
    groupBinMap_.clear();
    binCount_ = 0;
  }
  // Adds the given flowFile to the first available bin in which it fits for the given group or creates a new bin in the specified group if necessary.
  bool offer(const std::string &group, std::shared_ptr<core::FlowFile> flow);
  // gather ready bins once the bin are full enough or exceed bin age
  void gatherReadyBins();
  // marks oldest bin as ready
  void removeOldestBin();
  // get ready bin from binManager
  void getReadyBin(std::deque<std::unique_ptr<Bin>> &retBins);

 private:
  std::mutex mutex_;
  uint64_t minSize_{0};
  uint64_t maxSize_{std::numeric_limits<decltype(maxSize_)>::max()};
  uint32_t maxEntries_{std::numeric_limits<decltype(maxEntries_)>::max()};
  uint32_t minEntries_{1};
  std::string fileCount_;
  // Bin Age in msec
  uint64_t binAge_{std::numeric_limits<decltype(binAge_)>::max()};
  std::map<std::string, std::unique_ptr<std::deque<std::unique_ptr<Bin>>> >groupBinMap_;
  std::deque<std::unique_ptr<Bin>> readyBin_;
  int binCount_{0};
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<BinManager>::getLogger()};
};

// BinFiles Class
class BinFiles : public core::Processor {
 protected:
  static core::Relationship Self;

 public:
  using core::Processor::Processor;
  // Destructor
  virtual ~BinFiles() = default;
  // Processor Name
  static constexpr char const* ProcessorName = "BinFiles";
  // Supported Properties
  EXTENSIONAPI static core::Property MinSize;
  EXTENSIONAPI static core::Property MaxSize;
  EXTENSIONAPI static core::Property MinEntries;
  EXTENSIONAPI static core::Property MaxEntries;
  EXTENSIONAPI static core::Property MaxBinCount;
  EXTENSIONAPI static core::Property MaxBinAge;
  EXTENSIONAPI static core::Property BatchSize;

  // Supported Relationships
  EXTENSIONAPI static core::Relationship Failure;
  EXTENSIONAPI static core::Relationship Original;

  // attributes
  EXTENSIONAPI static const char *FRAGMENT_ID_ATTRIBUTE;
  EXTENSIONAPI static const char *FRAGMENT_INDEX_ATTRIBUTE;
  EXTENSIONAPI static const char *FRAGMENT_COUNT_ATTRIBUTE;

  EXTENSIONAPI static const char *SEGMENT_ID_ATTRIBUTE;
  EXTENSIONAPI static const char *SEGMENT_INDEX_ATTRIBUTE;
  EXTENSIONAPI static const char *SEGMENT_COUNT_ATTRIBUTE;
  EXTENSIONAPI static const char *SEGMENT_ORIGINAL_FILENAME;
  EXTENSIONAPI static const char *TAR_PERMISSIONS_ATTRIBUTE;

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  // OnTrigger method, implemented by NiFi BinFiles
  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/) override {
  }
  // OnTrigger method, implemented by NiFi BinFiles
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  // Initialize, over write by NiFi BinFiles
  void initialize(void) override;

  void restore(const std::shared_ptr<core::FlowFile>& flowFile) override;

  std::set<std::shared_ptr<core::Connectable>> getOutGoingConnections(const std::string &relationship) const override;

 protected:
  // Allows general pre-processing of a flow file before it is offered to a bin. This is called before getGroupId().
  virtual void preprocessFlowFile(core::ProcessContext *context, core::ProcessSession *session, std::shared_ptr<core::FlowFile> flow);
  // Returns a group ID representing a bin. This allows flow files to be binned into like groups
  virtual std::string getGroupId(core::ProcessContext* /*context*/, std::shared_ptr<core::FlowFile> /*flow*/) {
    return "";
  }
  // Processes a single bin.
  virtual bool processBin(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/, std::unique_ptr<Bin>& /*bin*/) {
    return false;
  }
  // transfer flows to failure in bin
  void transferFlowsToFail(core::ProcessContext *context, core::ProcessSession *session, std::unique_ptr<Bin> &bin);
  // moves owned flows to session
  void addFlowsToSession(core::ProcessContext *context, core::ProcessSession *session, std::unique_ptr<Bin> &bin);

  BinManager binManager_;

 private:
  class FlowFileStore{
   public:
    /**
     * Returns the already-preprocessed FlowFiles that got restored on restart from the FlowFileRepository
     * @return the resurrected persisted FlowFiles
     */
    std::unordered_set<std::shared_ptr<core::FlowFile>> getNewFlowFiles();
    void put(const std::shared_ptr<core::FlowFile>& flowFile);
   private:
    std::atomic_bool has_new_flow_file_{false};
    std::mutex flow_file_mutex_;
    std::unordered_set<std::shared_ptr<core::FlowFile>> incoming_files_;
  };

  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<BinFiles>::getLogger()};
  uint32_t batchSize_{1};
  uint32_t maxBinCount_{100};
  FlowFileStore file_store_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

