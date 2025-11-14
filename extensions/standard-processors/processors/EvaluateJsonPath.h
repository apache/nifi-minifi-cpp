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
#include <string_view>
#include <array>

#include "core/ProcessorImpl.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "minifi-cpp/core/RelationshipDefinition.h"

#include "jsoncons/json.hpp"

namespace org::apache::nifi::minifi::processors::evaluate_json_path {
enum class DestinationType {
  FlowFileContent,
  FlowFileAttribute
};

enum class NullValueRepresentationOption {
  EmptyString,
  Null
};

enum class ReturnTypeOption {
  AutoDetect,
  JSON,
  Scalar
};

enum class PathNotFoundBehaviorOption {
  Warn,
  Ignore,
  Skip
};
}  // namespace org::apache::nifi::minifi::processors::evaluate_json_path

namespace magic_enum::customize {
using DestinationType = org::apache::nifi::minifi::processors::evaluate_json_path::DestinationType;
using NullValueRepresentationOption = org::apache::nifi::minifi::processors::evaluate_json_path::NullValueRepresentationOption;
using ReturnTypeOption = org::apache::nifi::minifi::processors::evaluate_json_path::ReturnTypeOption;
using PathNotFoundBehaviorOption = org::apache::nifi::minifi::processors::evaluate_json_path::PathNotFoundBehaviorOption;

template <>
constexpr customize_t enum_name<DestinationType>(DestinationType value) noexcept {
  switch (value) {
    case DestinationType::FlowFileContent:
      return "flowfile-content";
    case DestinationType::FlowFileAttribute:
      return "flowfile-attribute";
  }
  return invalid_tag;
}

template <>
constexpr customize_t enum_name<NullValueRepresentationOption>(NullValueRepresentationOption value) noexcept {
  switch (value) {
    case NullValueRepresentationOption::EmptyString:
      return "empty string";
    case NullValueRepresentationOption::Null:
      return "the string 'null'";
  }
  return invalid_tag;
}

template <>
constexpr customize_t enum_name<ReturnTypeOption>(ReturnTypeOption value) noexcept {
  switch (value) {
    case ReturnTypeOption::AutoDetect:
      return "auto-detect";
    case ReturnTypeOption::JSON:
      return "json";
    case ReturnTypeOption::Scalar:
      return "scalar";
  }
  return invalid_tag;
}

template <>
constexpr customize_t enum_name<PathNotFoundBehaviorOption>(PathNotFoundBehaviorOption value) noexcept {
  switch (value) {
    case PathNotFoundBehaviorOption::Warn:
      return "warn";
    case PathNotFoundBehaviorOption::Ignore:
      return "ignore";
    case PathNotFoundBehaviorOption::Skip:
      return "skip";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::processors {

class EvaluateJsonPath final : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Evaluates one or more JsonPath expressions against the content of a FlowFile. The results of those expressions are assigned to "
        "FlowFile Attributes or are written to the content of the FlowFile itself, depending on configuration of the Processor. JsonPaths are entered by adding user-defined properties; "
        "the name of the property maps to the Attribute Name into which the result will be placed (if the Destination is flowfile-attribute; otherwise, the property name is ignored). "
        "The value of the property must be a valid JsonPath expression. A Return Type of 'auto-detect' will make a determination based off the configured destination. When 'Destination' is set to "
        "'flowfile-attribute,' a return type of 'scalar' will be used. When 'Destination' is set to 'flowfile-content,' a return type of 'JSON' will be used.If the JsonPath evaluates to a JSON "
        "array or JSON object and the Return Type is set to 'scalar' the FlowFile will be unmodified and will be routed to failure. A Return Type of JSON can return scalar values if the provided "
        "JsonPath evaluates to the specified value and will be routed as a match.If Destination is 'flowfile-content' and the JsonPath does not evaluate to a defined path, the FlowFile will be "
        "routed to 'unmatched' without having its contents modified. If Destination is 'flowfile-attribute' and the expression matches nothing, attributes will be created with empty strings as the "
        "value unless 'Path Not Found Behaviour' is set to 'skip', and the FlowFile will always be routed to 'matched.'";

  EXTENSIONAPI static constexpr auto Destination = core::PropertyDefinitionBuilder<2>::createProperty("Destination")
      .withDescription("Indicates whether the results of the JsonPath evaluation are written to the FlowFile content or a FlowFile attribute.")
      .withAllowedValues(magic_enum::enum_names<evaluate_json_path::DestinationType>())
      .withDefaultValue(magic_enum::enum_name(evaluate_json_path::DestinationType::FlowFileAttribute))
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto NullValueRepresentation = core::PropertyDefinitionBuilder<2>::createProperty("Null Value Representation")
      .withDescription("Indicates the desired representation of JSON Path expressions resulting in a null value.")
      .withAllowedValues(magic_enum::enum_names<evaluate_json_path::NullValueRepresentationOption>())
      .withDefaultValue(magic_enum::enum_name(evaluate_json_path::NullValueRepresentationOption::EmptyString))
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto PathNotFoundBehavior = core::PropertyDefinitionBuilder<3>::createProperty("Path Not Found Behavior")
      .withDescription("Indicates how to handle missing JSON path expressions when destination is set to 'flowfile-attribute'. Selecting 'warn' will generate a warning when a JSON path expression is "
          "not found. Selecting 'skip' will omit attributes for any unmatched JSON path expressions.")
      .withAllowedValues(magic_enum::enum_names<evaluate_json_path::PathNotFoundBehaviorOption>())
      .withDefaultValue(magic_enum::enum_name(evaluate_json_path::PathNotFoundBehaviorOption::Ignore))
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto ReturnType = core::PropertyDefinitionBuilder<3>::createProperty("Return Type")
      .withDescription("Indicates the desired return type of the JSON Path expressions. Selecting 'auto-detect' will set the return type to 'json' for a Destination of 'flowfile-content', and "
          "'scalar' for a Destination of 'flowfile-attribute'.")
      .withAllowedValues(magic_enum::enum_names<evaluate_json_path::ReturnTypeOption>())
      .withDefaultValue(magic_enum::enum_name(evaluate_json_path::ReturnTypeOption::AutoDetect))
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Destination,
      NullValueRepresentation,
      PathNotFoundBehavior,
      ReturnType
  });

  EXTENSIONAPI static constexpr core::RelationshipDefinition Failure{"failure", "FlowFiles are routed to this relationship when the JsonPath cannot be evaluated against the content of the FlowFile; "
      "for instance, if the FlowFile is not valid JSON"};
  EXTENSIONAPI static constexpr core::RelationshipDefinition Matched{"matched", "FlowFiles are routed to this relationship when the JsonPath is successfully evaluated and the FlowFile is modified "
      "as a result"};
  EXTENSIONAPI static constexpr core::RelationshipDefinition Unmatched{"unmatched", "FlowFiles are routed to this relationship when the JsonPath does not match the content of the FlowFile and the "
      "Destination is set to flowfile-content"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Failure, Matched, Unmatched};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr auto EvaluationResult = core::DynamicProperty{"Evaluation Result", "JsonPath expression to evaluate", "Dynamic property values are evaluated as JsonPaths. "
      "In case of 'flowfile-content' destination, only one dynamic property with JsonPath may be specified, in this case the name of the property is ignored. "
      "In case of 'flowfile-attribute' destination, the result of the JsonPath evaluation is written to the attribute matching the dynamic property name.", true};
  EXTENSIONAPI static constexpr auto DynamicProperties = std::array{EvaluationResult};

  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  using ProcessorImpl::ProcessorImpl;

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::string extractQueryResult(const jsoncons::json& query_result) const;
  void writeQueryResult(core::ProcessSession& session, core::FlowFile& flow_file, const jsoncons::json& query_result, const std::string& property_name,
    std::unordered_map<std::string, std::string>& attributes_to_set) const;

  evaluate_json_path::DestinationType destination_ = evaluate_json_path::DestinationType::FlowFileAttribute;
  evaluate_json_path::NullValueRepresentationOption null_value_representation_ = evaluate_json_path::NullValueRepresentationOption::EmptyString;
  evaluate_json_path::PathNotFoundBehaviorOption path_not_found_behavior_ = evaluate_json_path::PathNotFoundBehaviorOption::Ignore;
  evaluate_json_path::ReturnTypeOption return_type_ = evaluate_json_path::ReturnTypeOption::AutoDetect;
};

}  // namespace org::apache::nifi::minifi::processors
