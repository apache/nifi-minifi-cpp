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

#include <atomic>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "core/state/nodes/MetricsBase.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/OptionalUtils.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class RetryFlowFile : public core::Processor {
 public:
  explicit RetryFlowFile(std::string name, const utils::Identifier& uuid = {})
      : Processor(std::move(name), uuid) {}
  ~RetryFlowFile() override = default;

  EXTENSIONAPI static constexpr const char* Description = "FlowFiles passed to this Processor have a 'Retry Attribute' value checked against a configured 'Maximum Retries' value. "
      "If the current attribute value is below the configured maximum, the FlowFile is passed to a retry relationship. "
      "The FlowFile may or may not be penalized in that condition. If the FlowFile's attribute value exceeds the configured maximum, "
      "the FlowFile will be passed to a 'retries_exceeded' relationship. "
      "WARNING: If the incoming FlowFile has a non-numeric value in the configured 'Retry Attribute' attribute, it will be reset to '1'. "
      "You may choose to fail the FlowFile instead of performing the reset. Additional dynamic properties can be defined for any attributes "
      "you wish to add to the FlowFiles transferred to 'retries_exceeded'. These attributes support attribute expression language.";

  EXTENSIONAPI static const core::Property RetryAttribute;
  EXTENSIONAPI static const core::Property MaximumRetries;
  EXTENSIONAPI static const core::Property PenalizeRetries;
  EXTENSIONAPI static const core::Property FailOnNonNumericalOverwrite;
  EXTENSIONAPI static const core::Property ReuseMode;
  static auto properties() {
    return std::array{
      RetryAttribute,
      MaximumRetries,
      PenalizeRetries,
      FailOnNonNumericalOverwrite,
      ReuseMode
    };
  }

  EXTENSIONAPI static const core::Relationship Retry;
  EXTENSIONAPI static const core::Relationship RetriesExceeded;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() {
    return std::array{
      Retry,
      RetriesExceeded,
      Failure
    };
  }

  EXTENSIONAPI static const core::OutputAttribute RetryOutputAttribute;
  EXTENSIONAPI static const core::OutputAttribute RetryWithUuidOutputAttribute;
  static auto outputAttributes() {
    return std::array{
      RetryOutputAttribute,
      RetryWithUuidOutputAttribute
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static const core::DynamicProperty RetriesExceededAttribute;
  static auto dynamicProperties() { return std::array{RetriesExceededAttribute}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  // ReuseMode allowable values
  EXTENSIONAPI static constexpr char const* FAIL_ON_REUSE = "Fail on Reuse";
  EXTENSIONAPI static constexpr char const* WARN_ON_REUSE = "Warn on Reuse";
  EXTENSIONAPI static constexpr char const*   RESET_REUSE = "Reset Reuse";

  void onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /* sessionFactory */) override;
  void onTrigger(core::ProcessContext* context, core::ProcessSession* session) override;
  void initialize() override;

 private:
  void readDynamicPropertyKeys(core::ProcessContext* context);
  std::optional<uint64_t> getRetryPropertyValue(const std::shared_ptr<core::FlowFile>& flow_file) const;
  void setRetriesExceededAttributesOnFlowFile(core::ProcessContext* context, const std::shared_ptr<core::FlowFile>& flow_file) const;

  std::string retry_attribute_;
  uint64_t maximum_retries_ = 3;  // The real default value is set by the default on the MaximumRetries property
  bool penalize_retries_ =  true;  // The real default value is set by the default on the PenalizeRetries property
  bool fail_on_non_numerical_overwrite_ = false;  // The real default value is set by the default on the FailOnNonNumericalOverwrite property
  std::string reuse_mode_;
  std::vector<core::Property> exceeded_flowfile_attribute_keys_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RetryFlowFile>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
