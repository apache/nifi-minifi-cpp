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

#include <vector>
#include <string>
#include <set>
#include <unordered_set>
#include <memory>
#include <map>

#include "rapidjson/document.h"
#include "core/FlowFile.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Processor.h"
#include "core/Property.h"
#include "io/StreamPipe.h"
#include "utils/Enum.h"
#include "utils/Export.h"
#include "utils/RegexUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class AttributesToJSON : public core::Processor {
 public:
  // Supported Properties
  EXTENSIONAPI static const core::Property AttributesList;
  EXTENSIONAPI static const core::Property AttributesRegularExpression;
  EXTENSIONAPI static const core::Property Destination;
  EXTENSIONAPI static const core::Property IncludeCoreAttributes;
  EXTENSIONAPI static const core::Property NullValue;

  // Supported Relationships
  EXTENSIONAPI static const core::Relationship Success;

  SMART_ENUM(WriteDestination,
    (FLOWFILE_ATTRIBUTE, "flowfile-attribute"),
    (FLOWFILE_CONTENT, "flowfile-content")
  )

  explicit AttributesToJSON(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid) {
  }

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

 private:
  bool isCoreAttributeToBeFiltered(const std::string& attribute) const;
  std::optional<std::unordered_set<std::string>> getAttributesToBeWritten(const core::FlowFile::AttributeMap& flowfile_attributes) const;
  void addAttributeToJson(rapidjson::Document& document, const std::string& key, const std::optional<std::string>& value);
  std::string buildAttributeJsonData(const core::FlowFile::AttributeMap& flowfile_attributes);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AttributesToJSON>::getLogger();
  std::vector<std::string> attribute_list_;
  std::optional<utils::Regex> attributes_regular_expression_;
  WriteDestination write_destination_;
  bool include_core_attributes_ = true;
  bool null_value_ = false;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
