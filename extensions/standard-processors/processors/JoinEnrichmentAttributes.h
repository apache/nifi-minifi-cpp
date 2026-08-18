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

#include <array>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/FlowFileStore.h"
#include "core/ProcessorImpl.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "utils/Enum.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::standard {

namespace join_enrichment_attributes {
class TimeOutTracker {
 public:
  explicit TimeOutTracker(std::chrono::steady_clock::duration timeout) : time_out_(timeout) {
  }
  TimeOutTracker(const TimeOutTracker&) = delete;
  TimeOutTracker& operator=(const TimeOutTracker&) = delete;
  TimeOutTracker(TimeOutTracker&&) = delete;
  TimeOutTracker& operator=(TimeOutTracker&&) = delete;
  ~TimeOutTracker() = default;

  void track(std::string id, std::chrono::steady_clock::time_point timestamp) {
    queue_.emplace_back(timestamp, std::move(id));
  }

  std::vector<std::string> getTimedOutFlowFiles(std::chrono::steady_clock::time_point current_time) {
    std::vector<std::string> result;
    // Even with 0 time_out_, we won't return just added FlowFiles
    while (!queue_.empty() && queue_.front().timestamp + time_out_ < current_time) {
      result.push_back(std::move(queue_.front().group_name));
      queue_.pop_front();
    }
    return result;
  }

 private:
  struct TimeStampedGroup {
    std::chrono::steady_clock::time_point timestamp;
    std::string group_name;
  };

  std::chrono::steady_clock::duration time_out_;
  std::deque<TimeStampedGroup> queue_;
};
}  // namespace join_enrichment_attributes

using StoredFlowFileMap = std::unordered_map<std::string, std::shared_ptr<core::FlowFile>, utils::string::transparent_string_hash, std::equal_to<>>;

class JoinEnrichmentAttributes : public core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  EXTENSIONAPI static constexpr const char* Description =
      "Rejoins the forked FlowFiles coming from ForkEnrichment processor, the resulting FlowFile will have the Original's content and all attributes "
      "from both of them (prioritizing Enrichment's).";

  EXTENSIONAPI static constexpr auto Invalid = core::RelationshipDefinition{"invalid",
      "Any FlowFiles without the requisite attributes will be routed here"};
  EXTENSIONAPI static constexpr auto Joined = core::RelationshipDefinition{"joined",
      "The resultant FlowFile with Records joined together from both the original and enrichment FlowFiles will be routed to this relationship"};
  EXTENSIONAPI static constexpr auto Original = core::RelationshipDefinition{"original",
      "Both of the incoming FlowFiles ('original' and 'enrichment') will be routed to this Relationship. I.e., this is the 'original' version of "
      "both of these FlowFiles."};
  EXTENSIONAPI static constexpr auto TimeoutRelationship = core::RelationshipDefinition{"timeout",
      "If one of the incoming FlowFiles (i.e., the 'original' FlowFile or the 'enrichment' FlowFile) arrives to this Processor but the other does "
      "not arrive within the configured Timeout period, the FlowFile that did arrive is routed to this relationship."};

  EXTENSIONAPI static constexpr auto MaxBatchSize =
      core::PropertyDefinitionBuilder<>::createProperty("Max Batch Size")
          .withDescription("The maximum number of flow files to process at a time. If unset, all FlowFiles will be processed at once.")
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .build();

  EXTENSIONAPI static constexpr auto TimeoutProperty =
      core::PropertyDefinitionBuilder<>::createProperty("Timeout")
          .withDescription(
              "Specifies the maximum amount of time to wait for the second FlowFile once the first arrives at the processor, after which point the "
              "first FlowFile will be routed to the 'timeout' relationship.")
          .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
          .isRequired(false)
          .build();

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 2>{TimeoutProperty, MaxBatchSize};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Invalid, Joined, Original, TimeoutRelationship};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr auto InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  EXTENSIONAPI static const core::Relationship Self;

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void restore(const std::shared_ptr<core::FlowFile>& flowFile) override;

 private:
  enum class EnrichmentRole {
    ORIGINAL,
    ENRICHMENT,
  };

  void handleFlowFile(std::shared_ptr<core::FlowFile> flow_file, core::ProcessSession& session, std::chrono::steady_clock::time_point current_time);
  void join(const std::shared_ptr<core::FlowFile>& original, const std::shared_ptr<core::FlowFile>& enrichment, core::ProcessSession& session) const;

  core::FlowFileStore flow_file_store_;
  // We need to track current session's FlowFiles (we cant add those)
  std::unordered_set<utils::Identifier> session_flow_files_;

  std::optional<join_enrichment_attributes::TimeOutTracker> time_out_tracker_;
  StoredFlowFileMap originals_;
  StoredFlowFileMap enrichments_;
  std::optional<uint64_t> max_batch_size_;
};
}  // namespace org::apache::nifi::minifi::standard
