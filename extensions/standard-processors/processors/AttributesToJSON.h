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
#include "core/logging/LoggerConfiguration.h"
#include "core/Processor.h"
#include "core/Property.h"
#include "io/StreamPipe.h"
#include "utils/Enum.h"
#include "utils/Export.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

class AttributesToJSON : public core::Processor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Generates a JSON representation of the input FlowFile Attributes. "
      "The resulting JSON can be written to either a new Attribute 'JSONAttributes' or written to the FlowFile as content.";

  EXTENSIONAPI static const core::Property AttributesList;
  EXTENSIONAPI static const core::Property AttributesRegularExpression;
  EXTENSIONAPI static const core::Property Destination;
  EXTENSIONAPI static const core::Property IncludeCoreAttributes;
  EXTENSIONAPI static const core::Property NullValue;
  static auto properties() {
    return std::array{
      AttributesList,
      AttributesRegularExpression,
      Destination,
      IncludeCoreAttributes,
      NullValue
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  SMART_ENUM(WriteDestination,
    (FLOWFILE_ATTRIBUTE, "flowfile-attribute"),
    (FLOWFILE_CONTENT, "flowfile-content")
  )

  explicit AttributesToJSON(std::string name, const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid) {
  }

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;

 private:
  bool isCoreAttributeToBeFiltered(const std::string& attribute) const;
  std::optional<std::unordered_set<std::string>> getAttributesToBeWritten(const core::FlowFile::AttributeMap& flowfile_attributes) const;
  void addAttributeToJson(rapidjson::Document& document, const std::string& key, const std::optional<std::string>& value) const;
  std::string buildAttributeJsonData(const core::FlowFile::AttributeMap& flowfile_attributes);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AttributesToJSON>::getLogger(uuid_);
  std::vector<std::string> attribute_list_;
  std::optional<utils::Regex> attributes_regular_expression_;
  WriteDestination write_destination_;
  bool include_core_attributes_ = true;
  bool null_value_ = false;
};

}  // namespace org::apache::nifi::minifi::processors
