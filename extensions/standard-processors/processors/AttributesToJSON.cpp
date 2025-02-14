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
#include "core/ProcessSession.h"
#include "core/ProcessContext.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

void AttributesToJSON::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void AttributesToJSON::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  attribute_list_ = context.getProperty(AttributesList)
      | utils::transform([](const auto attributes_list_str) { return utils::string::splitAndTrimRemovingEmpty(attributes_list_str, ","); })
      | utils::valueOrElse([] { return std::vector<std::string>{}; });

  attributes_regular_expression_ = context.getProperty(AttributesRegularExpression)
      | utils::transform([](const auto s) { return utils::Regex{s}; })
      | utils::toOptional();

  write_destination_ = utils::parseEnumProperty<attributes_to_json::WriteDestination>(context, Destination);

  include_core_attributes_ = context.getProperty(IncludeCoreAttributes)
      | utils::andThen(parsing::parseBool)
      | utils::expect("AttributesToJSON::IncludeCoreAttributes should be available in onSchedule");
  null_value_ = context.getProperty(NullValue)
      | utils::andThen(parsing::parseBool)
      | utils::expect("AttributesToJSON::NullValue should be available in onSchedule");
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
      if (utils::regexMatch(key, attributes_regular_expression_.value())) {
        attributes.insert(key);
      }
    }
  }

  return attributes;
}

void AttributesToJSON::addAttributeToJson(rapidjson::Document& document, const std::string& key, const std::optional<std::string>& value) const {
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

void AttributesToJSON::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (!flow_file) {
    return;
  }

  auto json_data = buildAttributeJsonData(*flow_file->getAttributesPtr());
  if (write_destination_ == attributes_to_json::WriteDestination::FLOWFILE_ATTRIBUTE) {
    logger_->log_debug("Writing the following attribute data to JSONAttributes attribute: {}", json_data);
    session.putAttribute(*flow_file, "JSONAttributes", json_data);
    session.transfer(flow_file, Success);
  } else {
    logger_->log_debug("Writing the following attribute data to flowfile: {}", json_data);
    session.writeBuffer(flow_file, json_data);
    session.transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(AttributesToJSON, Processor);

}  // namespace org::apache::nifi::minifi::processors
