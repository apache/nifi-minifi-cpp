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

#include "rapidjson/document.h"
#include "rapidjson/writer.h"

#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const std::set<std::string> AttributesToJSON::CORE_ATTRIBUTES({core::SpecialFlowAttribute::PATH, core::SpecialFlowAttribute::ABSOLUTE_PATH,
  core::SpecialFlowAttribute::FILENAME, core::SpecialFlowAttribute::UUID, core::SpecialFlowAttribute::priority, core::SpecialFlowAttribute::MIME_TYPE,
  core::SpecialFlowAttribute::DISCARD_REASON, core::SpecialFlowAttribute::ALTERNATE_IDENTIFIER, core::SpecialFlowAttribute::FLOW_ID});

const core::Property AttributesToJSON::AttributesList(
    core::PropertyBuilder::createProperty("Attributes List")
      ->withDescription("Comma separated list of attributes to be included in the resulting JSON. "
                        "If this value is left empty then all existing Attributes will be included. This list of attributes is case sensitive. "
                        "If an attribute specified in the list is not found it will be be emitted to the resulting JSON with an empty string or NULL value.")
      ->build());

const core::Property AttributesToJSON::AttributesRegularExpression(
    core::PropertyBuilder::createProperty("Attributes Regular Expression")
      ->withDescription("Regular expression that will be evaluated against the flow file attributes to select the matching attributes. "
                        "This property can be used in combination with the attributes list property.")
      ->build());

const core::Property AttributesToJSON::Destination(
    core::PropertyBuilder::createProperty("Destination")
      ->withDescription("Control if JSON value is written as a new flowfile attribute 'JSONAttributes' or written in the flowfile content. "
                        "Writing to flowfile content will overwrite any existing flowfile content.")
      ->isRequired(true)
      ->withDefaultValue<std::string>("flowfile-attribute")
      ->withAllowableValues<std::string>({"flowfile-attribute", "flowfile-content"})
      ->build());

const core::Property AttributesToJSON::IncludeCoreAttributes(
    core::PropertyBuilder::createProperty("Include Core Attributes")
      ->withDescription("Determines if the FlowFile core attributes which are contained in every FlowFile should be included in the final JSON value generated.")
      ->isRequired(true)
      ->withDefaultValue<bool>(true)
      ->build());

const core::Property AttributesToJSON::NullValue(
    core::PropertyBuilder::createProperty("Null Value")
      ->withDescription("If true a non existing or empty attribute will be NULL in the resulting JSON. If false an empty string will be placed in the JSON")
      ->isRequired(true)
      ->withDefaultValue<bool>(false)
      ->build());

core::Relationship AttributesToJSON::Success("success", "Successfully converted attributes to JSON");
core::Relationship AttributesToJSON::Failure("failure", "Failed to convert attributes to JSON");

void AttributesToJSON::initialize() {
  setSupportedProperties({
    AttributesList,
    AttributesRegularExpression,
    Destination,
    IncludeCoreAttributes,
    NullValue
  });
  setSupportedRelationships({Success, Failure});
}

void AttributesToJSON::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /*sessionFactory*/) {
  std::string attributes;
  context->getProperty(AttributesList.getName(), attributes);
  attribute_list_ = utils::StringUtils::split(attributes, ",");
  context->getProperty(AttributesRegularExpression.getName(), attributes_regular_expression_str_);
  if (!attributes_regular_expression_str_.empty()) {
    attributes_regular_expression_ = std::regex(attributes_regular_expression_str_);
  }
  context->getProperty(Destination.getName(), destination_);
  context->getProperty(IncludeCoreAttributes.getName(), include_core_attributes_);
  context->getProperty(NullValue.getName(), null_value_);
}

void AttributesToJSON::onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* session) {
  auto flow_file = session->get();
  if (!flow_file) {
    return;
  }

  auto attributes = flow_file->getAttributes();
  auto root = rapidjson::Document(rapidjson::kObjectType);

  auto addAttributeToJson = [this, &root](const std::string& key, const std::string& value) {
    if (!include_core_attributes_ && CORE_ATTRIBUTES.find(key) != CORE_ATTRIBUTES.end()) {
      return;
    }
    if (!attributes_regular_expression_str_.empty() && !std::regex_search(key, attributes_regular_expression_)) {
      return;
    }
    rapidjson::Value json_key(key.c_str(), root.GetAllocator());
    rapidjson::Value json_val;
    if (!value.empty() || !null_value_) {
      json_val.SetString(value.c_str(), root.GetAllocator());
    }
    root.AddMember(json_key, json_val, root.GetAllocator());
  };

  if (!attribute_list_.empty()) {
    for (const auto& attribute : attribute_list_) {
      addAttributeToJson(attribute, attributes[attribute]);
    }
  } else {
    for (const auto& kvp : attributes) {
      addAttributeToJson(kvp.first, kvp.second);
    }
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  root.Accept(writer);
  std::string json_data = buffer.GetString();

  if (destination_ == "flowfile-attribute") {
    logger_->log_debug("Writing the following attribute data to JSONAttributes attribute: %s", json_data);
    session->putAttribute(flow_file, "JSONAttributes", json_data);
  } else if (destination_ == "flowfile-content") {
    logger_->log_debug("Writing the following attribute data to flowfile: %s", json_data);
    AttributesToJSON::WriteCallback callback(json_data);
    session->write(flow_file, &callback);
  } else {
    logger_->log_error("Unimplemented destination was set in AttributesToJSON's Destination property");
    session->transfer(flow_file, Failure);
  }

  session->transfer(flow_file, Success);
}

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
