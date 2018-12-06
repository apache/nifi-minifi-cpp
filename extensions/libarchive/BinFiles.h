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
#ifndef __BIN_FILES_H__
#define __BIN_FILES_H__

#include <climits>
#include <deque>
#include <map>
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"

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
  explicit Bin(const uint64_t &minSize, const uint64_t &maxSize, const int &minEntries, const int & maxEntries, const std::string &fileCount, const std::string &groupId)
      : minSize_(minSize),
        maxSize_(maxSize),
        maxEntries_(maxEntries),
        minEntries_(minEntries),
        fileCount_(fileCount),
        groupId_(groupId),
        logger_(logging::LoggerFactory<Bin>::getLogger()) {
    queued_data_size_ = 0;
    creation_dated_ = getTimeMillis();
    std::shared_ptr<utils::IdGenerator> id_generator = utils::IdGenerator::getIdGenerator();
    id_generator->generate(uuid_);
    uuid_str_ = uuid_.to_string();
    logger_->log_debug("Bin %s for group %s created", uuid_str_, groupId_);
  }
  virtual ~Bin() {
    logger_->log_debug("Bin %s for group %s destroyed", uuid_str_, groupId_);
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
    uint64_t currentTime = getTimeMillis();
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
          int count = std::stoi(value);
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
    logger_->log_debug("Bin %s for group %s offer size %d byte %d min_entry %d max_entry %d", uuid_str_, groupId_, queue_.size(), queued_data_size_, minEntries_, maxEntries_);

    return true;
  }
  // getBinAge
  uint64_t getBinAge() {
    return creation_dated_;
  }
  int getSize() {
    return queue_.size();
  }
  // Get the UUID as string
  std::string getUUIDStr() {
    return uuid_str_;
  }
  std::string getGroupId() {
    return groupId_;
  }

 protected:

 private:
  uint64_t minSize_;
  uint64_t maxSize_;
  int maxEntries_;
  int minEntries_;
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
  // UUID string
  std::string uuid_str_;
};

// BinManager Class
class BinManager {
 public:
  // Constructor
  /*!
   * Create a new BinManager
   */
  BinManager()
      : minSize_(0),
        maxSize_(ULLONG_MAX),
        maxEntries_(INT_MAX),
        minEntries_(1),
        binAge_(ULLONG_MAX),
        binCount_(0),
        logger_(logging::LoggerFactory<BinManager>::getLogger()) {
  }
  virtual ~BinManager() {
    purge();
  }
  void setMinSize(const uint64_t &size) {
    minSize_ = size;
  }
  void setMaxSize(const uint64_t &size) {
    maxSize_ = size;
  }
  void setMaxEntries(const int &entries) {
    maxEntries_ = entries;
  }
  void setMinEntries(const int &entries) {
    minEntries_ = entries;
  }
  void setBinAge(const uint64_t &age) {
    binAge_ = age;
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
  // remove oldest bin
  void removeOldestBin();
  // get ready bin from binManager
  void getReadyBin(std::deque<std::unique_ptr<Bin>> &retBins);

 protected:

 private:
  std::mutex mutex_;
  uint64_t minSize_;
  uint64_t maxSize_;
  int maxEntries_;
  int minEntries_;
  std::string fileCount_;
  // Bin Age in msec
  uint64_t binAge_;
  std::map<std::string, std::unique_ptr<std::deque<std::unique_ptr<Bin>>> >groupBinMap_;
  std::deque<std::unique_ptr<Bin>> readyBin_;
  int binCount_;
  std::shared_ptr<logging::Logger> logger_;
};

// BinFiles Class
class BinFiles : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit BinFiles(std::string name, utils::Identifier uuid = utils::Identifier())
      : core::Processor(name, uuid),
        logger_(logging::LoggerFactory<BinFiles>::getLogger()) {
    maxBinCount_ = 100;
  }
  // Destructor
  virtual ~BinFiles() {
  }
  // Processor Name
  static constexpr char const* ProcessorName = "BinFiles";
  // Supported Properties
  static core::Property MinSize;
  static core::Property MaxSize;
  static core::Property MinEntries;
  static core::Property MaxEntries;
  static core::Property MaxBinCount;
  static core::Property MaxBinAge;

  // Supported Relationships
  static core::Relationship Failure;
  static core::Relationship Original;

  // attributes
  static const char *FRAGMENT_ID_ATTRIBUTE;
  static const char *FRAGMENT_INDEX_ATTRIBUTE;
  static const char *FRAGMENT_COUNT_ATTRIBUTE;

  static const char *SEGMENT_ID_ATTRIBUTE;
  static const char *SEGMENT_INDEX_ATTRIBUTE;
  static const char *SEGMENT_COUNT_ATTRIBUTE;
  static const char *SEGMENT_ORIGINAL_FILENAME;
  static const char *TAR_PERMISSIONS_ATTRIBUTE;

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);
  // OnTrigger method, implemented by NiFi BinFiles
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  }
  // OnTrigger method, implemented by NiFi BinFiles
  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);
  // Initialize, over write by NiFi BinFiles
  virtual void initialize(void);

 protected:
  // Allows general pre-processing of a flow file before it is offered to a bin. This is called before getGroupId().
  virtual void preprocessFlowFile(core::ProcessContext *context, core::ProcessSession *session, std::shared_ptr<core::FlowFile> flow);
  // Returns a group ID representing a bin. This allows flow files to be binned into like groups
  virtual std::string getGroupId(core::ProcessContext *context, std::shared_ptr<core::FlowFile> flow) {
    return "";
  }
  // Processes a single bin.
  virtual bool processBin(core::ProcessContext *context, core::ProcessSession *session, std::unique_ptr<Bin> &bin) {
    return false;
  }
  // transfer flows to failure in bin
  void transferFlowsToFail(core::ProcessContext *context, core::ProcessSession *session, std::unique_ptr<Bin> &bin);
  // add flows to session
  void addFlowsToSession(core::ProcessContext *context, core::ProcessSession *session, std::unique_ptr<Bin> &bin);

  BinManager binManager_;

 private:
  std::shared_ptr<logging::Logger> logger_;
  int maxBinCount_;
};

REGISTER_RESOURCE(BinFiles, "Bins flow files into buckets based on the number of entries or size of entries");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
