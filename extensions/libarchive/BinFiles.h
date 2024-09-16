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
#pragma once

#include <cinttypes>
#include <limits>
#include <deque>
#include <memory>
#include <unordered_set>
#include <string>
#include <set>
#include <map>
#include <utility>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "core/logging/LoggerFactory.h"
#include "utils/gsl.h"
#include "utils/Id.h"
#include "utils/Export.h"
#include "core/FlowFileStore.h"

namespace org::apache::nifi::minifi::processors {

class Bin {
 public:
  explicit Bin(const uint64_t &minSize, const uint64_t &maxSize, const size_t &minEntries, const size_t & maxEntries, std::string fileCount, std::string groupId)
      : minSize_(minSize),
        maxSize_(maxSize),
        maxEntries_(maxEntries),
        minEntries_(minEntries),
        fileCount_(std::move(fileCount)),
        groupId_(std::move(groupId)) {
    queued_data_size_ = 0;
    creation_dated_ = std::chrono::system_clock::now();
    uuid_ = utils::IdGenerator::getIdGenerator()->generate();
    logger_->log_debug("Bin {} for group {} created", getUUIDStr(), groupId_);
  }
  virtual ~Bin() {
    logger_->log_debug("Bin {} for group {} destroyed", getUUIDStr(), groupId_);
  }
  [[nodiscard]] bool isFull() const {
    return queued_data_size_ >= maxSize_ || queue_.size() >= maxEntries_;
  }
  // check whether the bin meet the min required size and entries so that it can be processed for merge
  [[nodiscard]] bool isReadyForMerge() const {
    return closed_ || isFull() || (queued_data_size_ >= minSize_ && queue_.size() >= minEntries_);
  }
  [[nodiscard]] bool isOlderThan(const std::chrono::milliseconds duration) const {
    return std::chrono::system_clock::now() > (creation_dated_ + duration);
  }
  std::deque<std::shared_ptr<core::FlowFile>>& getFlowFile() {
    return queue_;
  }
  bool offer(const std::shared_ptr<core::FlowFile>& flow) {
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

    if ((queued_data_size_ + flow->getSize()) > maxSize_ || (queue_.size() + 1) > maxEntries_) {
      closed_ = true;
      return false;
    }

    queue_.push_back(flow);
    queued_data_size_ += flow->getSize();
    logger_->log_debug("Bin {} for group {} offer size {} byte {} min_entry {} max_entry {}", getUUIDStr(), groupId_, queue_.size(), queued_data_size_, minEntries_, maxEntries_);

    return true;
  }
  [[nodiscard]] std::chrono::system_clock::time_point getCreationDate() const {
    return creation_dated_;
  }
  [[nodiscard]] int getSize() const {
    return gsl::narrow<int>(queue_.size());
  }
  [[nodiscard]] utils::SmallString<36> getUUIDStr() const {
    return uuid_.to_string();
  }
  [[nodiscard]] std::string getGroupId() const {
    return groupId_;
  }

 private:
  uint64_t minSize_;
  uint64_t maxSize_;
  size_t maxEntries_;
  size_t minEntries_;
  uint64_t queued_data_size_;
  bool closed_{false};
  std::deque<std::shared_ptr<core::FlowFile>> queue_;
  std::chrono::system_clock::time_point creation_dated_;
  std::string fileCount_;
  std::string groupId_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<Bin>::getLogger();
  utils::Identifier uuid_;
};

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
  void setBinAge(std::chrono::milliseconds age) {
    binAge_ = age;
  }
  [[nodiscard]] int getBinCount() const {
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
  bool offer(const std::string &group, const std::shared_ptr<core::FlowFile>& flow);
  // gather ready bins once the bin are full enough or exceed bin age
  void gatherReadyBins();
  // marks oldest bin as ready
  void removeOldestBin();
  void getReadyBin(std::deque<std::unique_ptr<Bin>> &retBins);
  void addReadyBin(std::unique_ptr<Bin> ready_bin);

 private:
  std::mutex mutex_;
  uint64_t minSize_{0};
  uint64_t maxSize_{std::numeric_limits<decltype(maxSize_)>::max()};
  uint32_t maxEntries_{std::numeric_limits<decltype(maxEntries_)>::max()};
  uint32_t minEntries_{1};
  std::string fileCount_;
  std::chrono::milliseconds binAge_{std::chrono::milliseconds::max()};
  std::map<std::string, std::unique_ptr<std::deque<std::unique_ptr<Bin>>> >groupBinMap_;
  std::deque<std::unique_ptr<Bin>> readyBin_;
  int binCount_{0};
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<BinManager>::getLogger()};
};

class BinFiles : public core::ProcessorImpl {
 protected:
  static const core::Relationship Self;

 public:
  using core::ProcessorImpl::ProcessorImpl;
  ~BinFiles() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Bins flow files into buckets based on the number of entries or size of entries";

  EXTENSIONAPI static constexpr auto MinSize = core::PropertyDefinitionBuilder<>::createProperty("Minimum Group Size")
      .withDescription("The minimum size of for the bundle")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("0")
      .build();
  EXTENSIONAPI static constexpr auto MaxSize = core::PropertyDefinitionBuilder<>::createProperty("Maximum Group Size")
      .withDescription("The maximum size for the bundle. If not specified, there is no maximum.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto MinEntries = core::PropertyDefinitionBuilder<>::createProperty("Minimum Number of Entries")
      .withDescription("The minimum number of files to include in a bundle")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("1")
      .build();
  EXTENSIONAPI static constexpr auto MaxEntries = core::PropertyDefinitionBuilder<>::createProperty("Maximum Number of Entries")
      .withDescription("The maximum number of files to include in a bundle. If not specified, there is no maximum.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto MaxBinAge = core::PropertyDefinitionBuilder<>::createProperty("Max Bin Age")
      .withDescription("The maximum age of a Bin that will trigger a Bin to be complete. Expected format is <duration> <time unit>")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto MaxBinCount = core::PropertyDefinitionBuilder<>::createProperty("Maximum number of Bins")
      .withDescription("Specifies the maximum number of bins that can be held in memory at any one time")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("100")
      .build();
  EXTENSIONAPI static constexpr auto BatchSize = core::PropertyDefinitionBuilder<>::createProperty("Batch Size")
      .withDescription("Maximum number of FlowFiles processed in a single session")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("1")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      MinSize,
      MaxSize,
      MinEntries,
      MaxEntries,
      MaxBinCount,
      MaxBinAge,
      BatchSize
  });


  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure",
      "If the bundle cannot be created, all FlowFiles that would have been used to create the bundle will be transferred to failure"};
  EXTENSIONAPI static constexpr auto Original = core::RelationshipDefinition{"original",
      "The FlowFiles that were used to create the bundle"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{
      Failure,
      Original
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  EXTENSIONAPI static const char *FRAGMENT_ID_ATTRIBUTE;
  EXTENSIONAPI static const char *FRAGMENT_INDEX_ATTRIBUTE;
  EXTENSIONAPI static const char *FRAGMENT_COUNT_ATTRIBUTE;

  EXTENSIONAPI static const char *SEGMENT_ID_ATTRIBUTE;
  EXTENSIONAPI static const char *SEGMENT_INDEX_ATTRIBUTE;
  EXTENSIONAPI static const char *SEGMENT_COUNT_ATTRIBUTE;
  EXTENSIONAPI static const char *SEGMENT_ORIGINAL_FILENAME;
  EXTENSIONAPI static const char *TAR_PERMISSIONS_ATTRIBUTE;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

  void restore(const std::shared_ptr<core::FlowFile>& flowFile) override;

  std::set<core::Connectable*> getOutGoingConnections(const std::string &relationship) override;

 protected:
  // Allows general pre-processing of a flow file before it is offered to a bin. This is called before getGroupId().
  virtual void preprocessFlowFile(const std::shared_ptr<core::FlowFile>& flow);
  // Returns a group ID representing a bin. This allows flow files to be binned into like groups
  virtual std::string getGroupId(const std::shared_ptr<core::FlowFile>& /*flow*/) {
    return "";
  }
  virtual bool processBin(core::ProcessSession& /*session*/, std::unique_ptr<Bin>& /*bin*/) {
    return false;
  }
  static void transferFlowsToFail(core::ProcessSession &session, std::unique_ptr<Bin> &bin);
  static void addFlowsToSession(core::ProcessSession &session, std::unique_ptr<Bin> &bin);

  // Sort flow files retrieved from the flow file repository after restart to their respective bins
  bool resurrectFlowFiles(core::ProcessSession &session);
  bool assumeOwnershipOfNextBatch(core::ProcessSession &session);
  std::deque<std::unique_ptr<Bin>> gatherReadyBins(core::ProcessContext &context);
  void processReadyBins(std::deque<std::unique_ptr<Bin>> ready_bins, core::ProcessSession &session);

  BinManager binManager_;

 private:
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<BinFiles>::getLogger(uuid_)};
  uint32_t batchSize_{1};
  uint32_t maxBinCount_{100};
  core::FlowFileStore file_store_;
};

}  // namespace org::apache::nifi::minifi::processors
