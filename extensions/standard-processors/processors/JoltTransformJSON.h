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

#include <string>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/Enum.h"
#include "utils/Searcher.h"
#include "../utils/JoltUtils.h"

namespace org::apache::nifi::minifi::processors::jolt_transform_json {
enum class JoltTransform {
  Shift
};
}  // namespace org::apache::nifi::minifi::processors::jolt_transform_json

namespace org::apache::nifi::minifi::processors {

class JoltTransformJSON : public core::ProcessorImpl {
 public:
  explicit JoltTransformJSON(std::string_view name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {}


  EXTENSIONAPI static constexpr const char* Description = "Applies a list of Jolt specifications to the flowfile JSON payload. A new FlowFile is created "
      "with transformed content and is routed to the 'success' relationship. If the JSON transform "
      "fails, the original FlowFile is routed to the 'failure' relationship.";

  EXTENSIONAPI static constexpr auto JoltTransform = core::PropertyDefinitionBuilder<magic_enum::enum_count<jolt_transform_json::JoltTransform>()>::createProperty("Jolt Transformation DSL")
      .withDescription("Specifies the Jolt Transformation that should be used with the provided specification.")
      .withDefaultValue(magic_enum::enum_name(jolt_transform_json::JoltTransform::Shift))
      .withAllowedValues(magic_enum::enum_names<jolt_transform_json::JoltTransform>())
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto JoltSpecification = core::PropertyDefinitionBuilder<>::createProperty("Jolt Specification")
      .withDescription("Jolt Specification for transformation of JSON data. The value for this property may be the text of a Jolt specification "
          "or the path to a file containing a Jolt specification. 'Jolt Specification' must be set, or "
          "the value is ignored if the Jolt Sort Transformation is selected.")
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      JoltTransform,
      JoltSpecification
  });

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success",
      "The FlowFile with transformed content will be routed to this relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure",
      "If a FlowFile fails processing for any reason (for example, the FlowFile is not valid JSON), it will be routed to this relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  jolt_transform_json::JoltTransform transform_;
  std::optional<utils::jolt::Spec> spec_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<JoltTransformJSON>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
