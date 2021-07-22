/**
 * @file AttributesToJSON.cpp
 * AttributesToJSON class implementation
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
#include "AttributesToJSON.h"

#include <unordered_set>

#include "rapidjson/writer.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "range/v3/algorithm/find.hpp"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const core::Property AttributesToJSON::AttributesList(
  core::PropertyBuilder::createProperty("Attributes List")
    ->withDescription("Comma separated list of attributes to be included in the resulting JSON. "
                      "If this value is left empty then all existing Attributes will be included. This list of attributes is case sensitive. "
                      "If an attribute specified in the list is not found it will be be emitted to the resulting JSON with an empty string or NULL value.")
    ->build());

const core::Property AttributesToJSON::AttributesRegularExpression(
  core::PropertyBuilder::createProperty("Attributes Regular Expression")
    ->withDescription("Regular expression that will be evaluated against the flow file attributes to select the matching attributes. "
                      "Both the matching attributes and the selected attributes from the Attributes List property will be written in the resulting JSON.")
    ->build());

const core::Property AttributesToJSON::Destination(
  core::PropertyBuilder::createProperty("Destination")
    ->withDescription("Control if JSON value is written as a new flowfile attribute 'JSONAttributes' or written in the flowfile content. "
                      "Writing to flowfile content will overwrite any existing flowfile content.")
    ->isRequired(true)
    ->withDefaultValue<std::string>(toString(WriteDestination::FLOWFILE_ATTRIBUTE))
    ->withAllowableValues<std::string>(WriteDestination::values())
    ->build());

const core::Property AttributesToJSON::IncludeCoreAttributes(
  core::PropertyBuilder::createProperty("Include Core Attributes")
    ->withDescription("Determines if the FlowFile core attributes which are contained in every FlowFile should be included in the final JSON value generated.")
    ->isRequired(true)
    ->withDefaultValue<bool>(true)
    ->build());

const core::Property AttributesToJSON::NullValue(
  core::PropertyBuilder::createProperty("Null Value")
    ->withDescription("If true a non existing selected attribute will be NULL in the resulting JSON. If false an empty string will be placed in the JSON.")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->build());

const core::Relationship AttributesToJSON::Success("success", "All FlowFiles received are routed to success");

void AttributesToJSON::initialize() {
  setSupportedProperties({
    AttributesList,
    AttributesRegularExpression,
    Destination,
    IncludeCoreAttributes,
    NullValue
  });
  setSupportedRelationships({Success});
}

void AttributesToJSON::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /*sessionFactory*/) {
  std::string value;
  if (context->getProperty(AttributesList.getName(), value) && !value.empty()) {
    attribute_list_ = utils::StringUtils::splitAndTrimRemovingEmpty(value, ",");
  }
  if (context->getProperty(AttributesRegularExpression.getName(), value) && !value.empty()) {
    attributes_regular_expression_ = std::regex(value);
  }
  write_destination_ = WriteDestination::parse(utils::parsePropertyWithAllowableValuesOrThrow(*context, Destination.getName(), WriteDestination::values()).c_str());
  context->getProperty(IncludeCoreAttributes.getName(), include_core_attributes_);
  context->getProperty(NullValue.getName(), null_value_);
}

bool AttributesToJSON::isCoreAttributeToBeFiltered(const std::string& attribute) const {
  const auto& special_attributes = core::SpecialFlowAttribute::getSpecialFlowAttributes();
  return !include_core_attributes_ && ranges::find(special_attributes, attribute) != ranges::end(special_attributes);
}

std::optional<std::unordered_set<std::string>> AttributesToJSON::getAttributesToBeWritten(const core::FlowFile::AttributeMap& flowfile_attributes) const {
  if (attribute_list_.empty() && !attributes_regular_expression_) {
    return std::nullopt;
  }

  std::unordered_set<std::string> attributes;

  for (const auto& attribute : attribute_list_) {
    attributes.insert(attribute);
  }

  if (attributes_regular_expression_) {
    for (const auto& [key, value] : flowfile_attributes) {
      if (std::regex_match(key, attributes_regular_expression_.value())) {
        attributes.insert(key);
      }
    }
  }

  return attributes;
}

void AttributesToJSON::addAttributeToJson(rapidjson::Document& document, const std::string& key, const std::optional<std::string>& value) {
  rapidjson::Value json_key(key.c_str(), document.GetAllocator());
  rapidjson::Value json_val;
  if (value || !null_value_) {
    json_val.SetString(value ? value->c_str() : "", document.GetAllocator());
  }
  document.AddMember(json_key, json_val, document.GetAllocator());
}

std::string AttributesToJSON::buildAttributeJsonData(const core::FlowFile::AttributeMap& flowfile_attributes) {
  auto root = rapidjson::Document(rapidjson::kObjectType);

  if (auto attributes_to_write = getAttributesToBeWritten(flowfile_attributes); attributes_to_write) {
    for (const auto& key : *attributes_to_write) {
      auto it = flowfile_attributes.find(key);
      addAttributeToJson(root, key, it == flowfile_attributes.end() ? std::nullopt : std::make_optional(it->second));
    }
  } else {
    for (const auto& [key, value] : flowfile_attributes) {
      if (!isCoreAttributeToBeFiltered(key)) {
        addAttributeToJson(root, key, value);
      }
    }
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  root.Accept(writer);
  return buffer.GetString();
}

void AttributesToJSON::onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* session) {
  auto flow_file = session->get();
  if (!flow_file) {
    return;
  }

  auto json_data = buildAttributeJsonData(*flow_file->getAttributesPtr());
  if (write_destination_ == WriteDestination::FLOWFILE_ATTRIBUTE) {
    logger_->log_debug("Writing the following attribute data to JSONAttributes attribute: %s", json_data);
    session->putAttribute(flow_file, "JSONAttributes", json_data);
    session->transfer(flow_file, Success);
  } else {
    logger_->log_debug("Writing the following attribute data to flowfile: %s", json_data);
    AttributesToJSON::WriteCallback callback(json_data);
    session->write(flow_file, &callback);
    session->transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(AttributesToJSON, "Generates a JSON representation of the input FlowFile Attributes. "
  "The resulting JSON can be written to either a new Attribute 'JSONAttributes' or written to the FlowFile as content.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
