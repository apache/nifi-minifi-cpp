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
#include <regex>

#include "rapidjson/document.h"
#include "core/Processor.h"
#include "core/Property.h"
#include "core/logging/Logger.h"
#include "utils/Enum.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class AttributesToJSON : public core::Processor {
 public:
  // Supported Properties
  static const core::Property AttributesList;
  static const core::Property AttributesRegularExpression;
  static const core::Property Destination;
  static const core::Property IncludeCoreAttributes;
  static const core::Property NullValue;

  // Supported Relationships
  static const core::Relationship Success;

  SMART_ENUM(WriteDestination,
    (FLOWFILE_ATTRIBUTE, "flowfile-attribute"),
    (FLOWFILE_CONTENT, "flowfile-content")
  )

  explicit AttributesToJSON(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid),
        logger_(logging::LoggerFactory<AttributesToJSON>::getLogger()) {
  }

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

 private:
  class WriteCallback : public OutputStreamCallback {
   public:
    explicit WriteCallback(const std::string& json_data) : json_data_(json_data) {}
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      const auto write_ret = stream->write(reinterpret_cast<const uint8_t*>(json_data_.data()), json_data_.length());
      return io::isError(write_ret) ? -1 : gsl::narrow<int64_t>(write_ret);
    }
   private:
    std::string json_data_;
  };

  bool isCoreAttributeToBeFiltered(const std::string& attribute) const;
  std::optional<std::unordered_set<std::string>> getAttributesToBeWritten(const core::FlowFile::AttributeMap& flowfile_attributes) const;
  void addAttributeToJson(rapidjson::Document& document, const std::string& key, const std::optional<std::string>& value);
  std::string buildAttributeJsonData(const core::FlowFile::AttributeMap& flowfile_attributes);

  std::shared_ptr<logging::Logger> logger_;
  std::vector<std::string> attribute_list_;
  std::optional<std::regex> attributes_regular_expression_;
  WriteDestination write_destination_;
  bool include_core_attributes_ = true;
  bool null_value_ = false;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
