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

#include "api/core/ProcessorImpl.h"
#include "api/utils/Export.h"
#include "api/utils/ProcessorConfigUtils.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-api.h"
#include "minifi-cpp/core/Annotation.h"

namespace org::apache::nifi::minifi::api_testing {

class PropertyTester : public api::core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Test processor to test the parsing of properties in the flow configuration";

  EXTENSIONAPI static constexpr auto OptionalPropertyWithDefaultValue =
      core::PropertyDefinitionBuilder<>::createProperty("OptionalPropertyWithDefaultValue")
          .withDescription("Test OptionalPropertyWithDefaultValue")
          .withDefaultValue("default_val")
          .isRequired(false)
          .build();

  EXTENSIONAPI static constexpr auto OptionalPropertyWithoutDefaultValue =
      core::PropertyDefinitionBuilder<>::createProperty("OptionalPropertyWithoutDefaultValue")
          .withDescription("Test OptionalPropertyWithoutDefaultValue")
          .isRequired(false)
          .build();

  EXTENSIONAPI static constexpr auto RequiredPropertyWithDefaultValue =
      core::PropertyDefinitionBuilder<>::createProperty("RequiredPropertyWithDefaultValue")
          .withDescription("Test RequiredPropertyWithDefaultValue")
          .withDefaultValue("default_val")
          .isRequired(true)
          .build();

  EXTENSIONAPI static constexpr auto RequiredPropertyWithoutDefaultValue =
      core::PropertyDefinitionBuilder<>::createProperty("RequiredPropertyWithoutDefaultValue")
          .withDescription("Test RequiredPropertyWithoutDefaultValue")
          .isRequired(true)
          .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>(
      {OptionalPropertyWithDefaultValue, OptionalPropertyWithoutDefaultValue, RequiredPropertyWithDefaultValue, RequiredPropertyWithoutDefaultValue});
  EXTENSIONAPI static constexpr auto Relationships = std::array<core::RelationshipDefinition, 0>{};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  using ProcessorImpl::ProcessorImpl;

 protected:
  minifi_status onTriggerImpl(api::core::ProcessContext& process_context, api::core::ProcessSession&) override {
    {
      const std::optional<std::string> optional_value_with_default = api::utils::parseOptionalProperty(process_context,
          OptionalPropertyWithDefaultValue);
      logger_->log_critical("OptionalPropertyWithDefaultValue: {}", optional_value_with_default);
    }
    {
      const std::optional<std::string> optional_value_without_default = api::utils::parseOptionalProperty(process_context,
          OptionalPropertyWithoutDefaultValue);
      logger_->log_critical("OptionalPropertyWithoutDefaultValue: {}", optional_value_without_default);
    }
    {
      const std::string required_value_with_default = api::utils::parseProperty(process_context, RequiredPropertyWithDefaultValue);
      logger_->log_critical("RequiredPropertyWithDefaultValue: {}", required_value_with_default);
    }
    {
      const std::string required_value_without_default = api::utils::parseProperty(process_context, RequiredPropertyWithoutDefaultValue);
      logger_->log_critical("RequiredPropertyWithoutDefaultValue: {}", required_value_without_default);
    }

    return MINIFI_STATUS_SUCCESS;
  }
  minifi_status onScheduleImpl(api::core::ProcessContext&) override {
    return MINIFI_STATUS_SUCCESS;
  }
};

}  // namespace org::apache::nifi::minifi::api_testing
