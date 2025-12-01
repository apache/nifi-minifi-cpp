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
#include <optional>

#include "core/ProcessorImpl.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "minifi-cpp/core/RelationshipDefinition.h"

#include "jsoncons/json.hpp"

namespace org::apache::nifi::minifi::processors::split_json {
enum class NullValueRepresentationOption {
  EmptyString,
  Null
};
}  // namespace org::apache::nifi::minifi::processors::split_json

namespace magic_enum::customize {
using NullValueRepresentationOption = org::apache::nifi::minifi::processors::split_json::NullValueRepresentationOption;

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
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::processors {

class SplitJson final : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description =
      "Splits a JSON File into multiple, separate FlowFiles for an array element specified by a JsonPath expression. Each generated FlowFile is comprised of an element of the specified array "
      "and transferred to relationship 'split,' with the original file transferred to the 'original' relationship. If the specified JsonPath is not found or does not evaluate to an array element, "
      "the original file is routed to 'failure' and no files are generated.";

  EXTENSIONAPI static constexpr auto JsonPathExpression = core::PropertyDefinitionBuilder<>::createProperty("JsonPath Expression")
      .withDescription("A JsonPath expression that indicates the array element to split into JSON/scalar fragments.")
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto NullValueRepresentation = core::PropertyDefinitionBuilder<2>::createProperty("Null Value Representation")
      .withDescription("Indicates the desired representation of JSON Path expressions resulting in a null value.")
      .withAllowedValues(magic_enum::enum_names<split_json::NullValueRepresentationOption>())
      .withDefaultValue(magic_enum::enum_name(split_json::NullValueRepresentationOption::EmptyString))
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      JsonPathExpression,
      NullValueRepresentation
  });

  EXTENSIONAPI static constexpr core::RelationshipDefinition Failure{"failure", "If a FlowFile fails processing for any reason (for example, the FlowFile is not valid JSON or the specified path "
      "does not exist), it will be routed to this relationship"};
  EXTENSIONAPI static constexpr core::RelationshipDefinition Original{"original", "The original FlowFile that was split into segments. If the FlowFile fails processing, nothing will be sent "
      "to this relationship"};
  EXTENSIONAPI static constexpr core::RelationshipDefinition Split{"split", "All segments of the original FlowFile will be routed to this relationship"};

  EXTENSIONAPI static constexpr auto Relationships = std::array{Failure, Original, Split};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  EXTENSIONAPI static constexpr auto FragmentIdentifier = core::OutputAttributeDefinition<2>{"fragment.identifier", {Split, Original},
    "All split FlowFiles produced from the same parent FlowFile will have the same randomly generated UUID added for this attribute"};
  EXTENSIONAPI static constexpr auto FragmentIndex = core::OutputAttributeDefinition<>{"fragment.index", {Split},
    "A one-up number that indicates the ordering of the split FlowFiles that were created from a single parent FlowFile"};
  EXTENSIONAPI static constexpr auto FragmentCount = core::OutputAttributeDefinition<2>{"fragment.count", {Split, Original},
    "The number of split FlowFiles generated from the parent FlowFile"};
  EXTENSIONAPI static constexpr auto SegmentOriginalFilename = core::OutputAttributeDefinition<>{"segment.original.filename", {Split},
    "The filename of the parent FlowFile"};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::to_array<core::OutputAttributeReference>({
    FragmentIdentifier,
    FragmentIndex,
    FragmentCount,
    SegmentOriginalFilename
  });

  using ProcessorImpl::ProcessorImpl;

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::optional<jsoncons::json> queryArrayUsingJsonPath(core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& flow) const;
  std::string jsonValueToString(const jsoncons::json& json_value) const;

  std::string json_path_expression_;
  split_json::NullValueRepresentationOption null_value_representation_ = split_json::NullValueRepresentationOption::EmptyString;
};

}  // namespace org::apache::nifi::minifi::processors
