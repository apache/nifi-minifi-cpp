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

#include "core/PropertyDefinitionBuilder.h"
#include "controllers/RecordSetWriter.h"
#include "core/FlowFile.h"
#include "core/ProcessSession.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::standard {
enum class OutputGroupingType {
  ARRAY,
  ONE_LINE_PER_OBJECT
};
}  // namespace org::apache::nifi::minifi::standard

namespace magic_enum::customize {
using OutputGroupingType = org::apache::nifi::minifi::standard::OutputGroupingType;

template <>
constexpr customize_t enum_name<OutputGroupingType>(OutputGroupingType value) noexcept {
  switch (value) {
    case OutputGroupingType::ARRAY:
      return "Array";
    case OutputGroupingType::ONE_LINE_PER_OBJECT:
      return "One Line Per Object";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::standard {

class JsonRecordSetWriter final : public core::RecordSetWriterImpl {
 public:
  explicit JsonRecordSetWriter(const std::string_view name, const utils::Identifier& uuid = {}) : RecordSetWriterImpl(name, uuid) {}

  JsonRecordSetWriter(JsonRecordSetWriter&&) = delete;
  JsonRecordSetWriter(const JsonRecordSetWriter&) = delete;
  JsonRecordSetWriter& operator=(JsonRecordSetWriter&&) = delete;
  JsonRecordSetWriter& operator=(const JsonRecordSetWriter&) = delete;

  ~JsonRecordSetWriter() override = default;

  EXTENSIONAPI static constexpr const char* Description =
      "Writes the results of a RecordSet as either a JSON Array or one JSON object per line. "
      "If using Array output, then even if the RecordSet consists of a single row, it will be written as an array with a single element. "
      "If using One Line Per Object output, the JSON objects cannot be pretty-printed.";

  EXTENSIONAPI static constexpr auto OutputGrouping = core::PropertyDefinitionBuilder<magic_enum::enum_count<OutputGroupingType>()>::createProperty("Output Grouping")
    .withDescription("Specifies how the writer should output the JSON records. "
                    "Note that if 'One Line Per Object' is selected, then Pretty Print JSON is ignored.")
    .withDefaultValue(magic_enum::enum_name(OutputGroupingType::ARRAY))
    .withAllowedValues(magic_enum::enum_names<OutputGroupingType>())
    .supportsExpressionLanguage(false)
    .isRequired(true)
    .build();

  EXTENSIONAPI static constexpr auto PrettyPrint = core::PropertyDefinitionBuilder<>::createProperty("Pretty Print JSON")
    .withDescription("Specifies whether or not the JSON should be pretty printed (only used when Array output is selected)")
    .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
    .withDefaultValue("false")
    .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
    OutputGrouping, PrettyPrint
  });

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void write(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) override;

  void initialize() override {
    setSupportedProperties(Properties);
  }
  void onEnable() override;
  void yield() override {}
  bool isRunning() const override { return getState() == core::controller::ControllerServiceState::ENABLED; }
  bool isWorkAvailable() override { return false; }

 private:
  void writeAsArray(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) const;
  static void writePerLine(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session);
  static void convertRecord(const core::Record& record, rapidjson::Value& record_json, rapidjson::Document::AllocatorType& alloc);

  OutputGroupingType output_grouping_ = OutputGroupingType::ARRAY;
  bool pretty_print_ = false;
};

}  // namespace org::apache::nifi::minifi::standard
