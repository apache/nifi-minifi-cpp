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
#include <string_view>
#include <utility>
#include <vector>

#include "core/state/nodes/MetricsBase.h"
#include "core/Core.h"
#include "core/OutputAttributeDefinition.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "core/logging/LoggerFactory.h"
#include "utils/OptionalUtils.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class RetryFlowFile : public core::ProcessorImpl {
 public:
  explicit RetryFlowFile(std::string_view name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {}
  ~RetryFlowFile() override = default;

  // ReuseMode allowed values
  EXTENSIONAPI static constexpr std::string_view FAIL_ON_REUSE = "Fail on Reuse";
  EXTENSIONAPI static constexpr std::string_view WARN_ON_REUSE = "Warn on Reuse";
  EXTENSIONAPI static constexpr std::string_view RESET_REUSE = "Reset Reuse";

  EXTENSIONAPI static constexpr const char* Description = "FlowFiles passed to this Processor have a 'Retry Attribute' value checked against a configured 'Maximum Retries' value. "
      "If the current attribute value is below the configured maximum, the FlowFile is passed to a retry relationship. "
      "The FlowFile may or may not be penalized in that condition. If the FlowFile's attribute value exceeds the configured maximum, "
      "the FlowFile will be passed to a 'retries_exceeded' relationship. "
      "WARNING: If the incoming FlowFile has a non-numeric value in the configured 'Retry Attribute' attribute, it will be reset to '1'. "
      "You may choose to fail the FlowFile instead of performing the reset. Additional dynamic properties can be defined for any attributes "
      "you wish to add to the FlowFiles transferred to 'retries_exceeded'. These attributes support attribute expression language.";

  EXTENSIONAPI static constexpr auto RetryAttribute = core::PropertyDefinitionBuilder<>::createProperty("Retry Attribute")
      .withDescription(
        "The name of the attribute that contains the current retry count for the FlowFile."
        "WARNING: If the name matches an attribute already on the FlowFile that does not contain a numerical value, "
        "the processor will either overwrite that attribute with '1' or fail based on configuration.")
      .withPropertyType(core::StandardPropertyTypes::NON_BLANK_TYPE)
      .withDefaultValue("flowfile.retries")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MaximumRetries = core::PropertyDefinitionBuilder<>::createProperty("Maximum Retries")
      .withDescription("The maximum number of times a FlowFile can be retried before being passed to the 'retries_exceeded' relationship.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("3")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto PenalizeRetries = core::PropertyDefinitionBuilder<>::createProperty("Penalize Retries")
      .withDescription("If set to 'true', this Processor will penalize input FlowFiles before passing them to the 'retry' relationship. This does not apply to the 'retries_exceeded' relationship.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto FailOnNonNumericalOverwrite = core::PropertyDefinitionBuilder<>::createProperty("Fail on Non-numerical Overwrite")
      .withDescription("If the FlowFile already has the attribute defined in 'Retry Attribute' that is *not* a number, fail the FlowFile instead of resetting that value to '1'")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto ReuseMode = core::PropertyDefinitionBuilder<3>::createProperty("Reuse Mode")
      .withDescription(
          "Defines how the Processor behaves if the retry FlowFile has a different retry UUID than "
          "the instance that received the FlowFile. This generally means that the attribute was "
          "not reset after being successfully retried by a previous instance of this processor.")
      .withAllowedValues({FAIL_ON_REUSE, WARN_ON_REUSE, RESET_REUSE})
      .withDefaultValue(FAIL_ON_REUSE)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      RetryAttribute,
      MaximumRetries,
      PenalizeRetries,
      FailOnNonNumericalOverwrite,
      ReuseMode
  });


  EXTENSIONAPI static constexpr auto Retry = core::RelationshipDefinition{"retry",
      "Input FlowFile has not exceeded the configured maximum retry count, pass this relationship back to the input Processor to create a limited feedback loop."};
  EXTENSIONAPI static constexpr auto RetriesExceeded = core::RelationshipDefinition{"retries_exceeded",
      "Input FlowFile has exceeded the configured maximum retry count, do not pass this relationship back to the input Processor to terminate the limited feedback loop."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure",
      "The processor is configured such that a non-numerical value on 'Retry Attribute' results in a failure instead of resetting "
      "that value to '1'. This will immediately terminate the limited feedback loop. Might also include when 'Maximum Retries' contains "
      " attribute expression language that does not resolve to an Integer."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{
      Retry,
      RetriesExceeded,
      Failure
  };

  EXTENSIONAPI static constexpr auto RetryOutputAttribute = core::OutputAttributeDefinition<0>{"Retry Attribute", {},
    "User defined retry attribute is updated with the current retry count"};
  EXTENSIONAPI static constexpr auto RetryWithUuidOutputAttribute = core::OutputAttributeDefinition<0>{"Retry Attribute .uuid", {},
    "User defined retry attribute with .uuid suffix is updated with the UUID of the processor that retried the FlowFile last"};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 2>{
    RetryOutputAttribute,
    RetryWithUuidOutputAttribute
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr auto RetriesExceededAttribute = core::DynamicProperty{"Exceeded FlowFile Attribute Key",
      "The value of the attribute added to the FlowFile",
      "One or more dynamic properties can be used to add attributes to FlowFiles passed to the 'retries_exceeded' relationship.",
      true};
  EXTENSIONAPI static constexpr auto DynamicProperties = std::array{RetriesExceededAttribute};

  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  void readDynamicPropertyKeys(const core::ProcessContext& context);
  std::optional<uint64_t> getRetryPropertyValue(const std::shared_ptr<core::FlowFile>& flow_file) const;
  void setRetriesExceededAttributesOnFlowFile(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) const;

  std::string retry_attribute_;
  uint64_t maximum_retries_ = 3;  // The real default value is set by the default on the MaximumRetries property
  bool penalize_retries_ =  true;  // The real default value is set by the default on the PenalizeRetries property
  bool fail_on_non_numerical_overwrite_ = false;  // The real default value is set by the default on the FailOnNonNumericalOverwrite property
  std::string reuse_mode_;
  std::vector<core::Property> exceeded_flowfile_attribute_keys_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RetryFlowFile>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
