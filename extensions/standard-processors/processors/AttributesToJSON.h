/**
 * @file AttributesToJSON.h
 * AttributesToJSON class declaration
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

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rapidjson/document.h"
#include "core/FlowFile.h"
#include "core/logging/LoggerFactory.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "io/StreamPipe.h"
#include "utils/Enum.h"
#include "utils/Export.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors::attributes_to_json {
enum class WriteDestination {
    FLOWFILE_ATTRIBUTE,
    FLOWFILE_CONTENT
};
}  // namespace org::apache::nifi::minifi::processors::attributes_to_json

namespace magic_enum::customize {
using WriteDestination = org::apache::nifi::minifi::processors::attributes_to_json::WriteDestination;

template <>
constexpr customize_t enum_name<WriteDestination>(WriteDestination value) noexcept {
  switch (value) {
    case WriteDestination::FLOWFILE_ATTRIBUTE:
      return "flowfile-attribute";
    case WriteDestination::FLOWFILE_CONTENT:
      return "flowfile-content";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::processors {

class AttributesToJSON : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Generates a JSON representation of the input FlowFile Attributes. "
      "The resulting JSON can be written to either a new Attribute 'JSONAttributes' or written to the FlowFile as content.";

  EXTENSIONAPI static constexpr auto AttributesList = core::PropertyDefinitionBuilder<>::createProperty("Attributes List")
      .withDescription("Comma separated list of attributes to be included in the resulting JSON. "
          "If this value is left empty then all existing Attributes will be included. This list of attributes is case sensitive. "
          "If an attribute specified in the list is not found it will be be emitted to the resulting JSON with an empty string or NULL value.")
      .build();
  EXTENSIONAPI static constexpr auto AttributesRegularExpression = core::PropertyDefinitionBuilder<>::createProperty("Attributes Regular Expression")
      .withDescription("Regular expression that will be evaluated against the flow file attributes to select the matching attributes. "
          "Both the matching attributes and the selected attributes from the Attributes List property will be written in the resulting JSON.")
      .build();
  EXTENSIONAPI static constexpr auto Destination = core::PropertyDefinitionBuilder<2>::createProperty("Destination")
      .withDescription("Control if JSON value is written as a new flowfile attribute 'JSONAttributes' or written in the flowfile content. "
          "Writing to flowfile content will overwrite any existing flowfile content.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(attributes_to_json::WriteDestination::FLOWFILE_ATTRIBUTE))
      .withAllowedValues(magic_enum::enum_names<attributes_to_json::WriteDestination>())
      .build();
  EXTENSIONAPI static constexpr auto IncludeCoreAttributes = core::PropertyDefinitionBuilder<>::createProperty("Include Core Attributes")
      .withDescription("Determines if the FlowFile core attributes which are contained in every FlowFile should be included in the final JSON value generated.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto NullValue = core::PropertyDefinitionBuilder<>::createProperty("Null Value")
      .withDescription("If true a non existing selected attribute will be NULL in the resulting JSON. If false an empty string will be placed in the JSON.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      AttributesList,
      AttributesRegularExpression,
      Destination,
      IncludeCoreAttributes,
      NullValue
  });


  EXTENSIONAPI static constexpr core::RelationshipDefinition Success{"success", "All FlowFiles received are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit AttributesToJSON(std::string_view name, const utils::Identifier& uuid = {})
      : core::ProcessorImpl(name, uuid) {
  }

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  bool isCoreAttributeToBeFiltered(const std::string& attribute) const;
  std::optional<std::unordered_set<std::string>> getAttributesToBeWritten(const core::FlowFile::AttributeMap& flowfile_attributes) const;
  void addAttributeToJson(rapidjson::Document& document, const std::string& key, const std::optional<std::string>& value) const;
  std::string buildAttributeJsonData(const core::FlowFile::AttributeMap& flowfile_attributes);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AttributesToJSON>::getLogger(uuid_);
  std::vector<std::string> attribute_list_;
  std::optional<utils::Regex> attributes_regular_expression_;
  attributes_to_json::WriteDestination write_destination_;
  bool include_core_attributes_ = true;
  bool null_value_ = false;
};

}  // namespace org::apache::nifi::minifi::processors
