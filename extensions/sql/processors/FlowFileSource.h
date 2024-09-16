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
#include <vector>
#include <memory>

#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "utils/Enum.h"
#include "data/SQLRowsetProcessor.h"
#include "core/ProcessSession.h"
#include "data/JSONSQLWriter.h"

namespace org::apache::nifi::minifi::processors::flow_file_source {
enum class OutputType {
  JSON,
  JSONPretty
};
}  // namespace org::apache::nifi::minifi::processors::flow_file_source

namespace magic_enum::customize {
using OutputType = org::apache::nifi::minifi::processors::flow_file_source::OutputType;

template <>
constexpr customize_t enum_name<OutputType>(OutputType value) noexcept {
  switch (value) {
    case OutputType::JSON:
      return "JSON";
    case OutputType::JSONPretty:
      return "JSON-Pretty";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::processors {

class FlowFileSource {
 public:
  EXTENSIONAPI static constexpr std::string_view FRAGMENT_IDENTIFIER = "fragment.identifier";
  EXTENSIONAPI static constexpr std::string_view FRAGMENT_COUNT = "fragment.count";
  EXTENSIONAPI static constexpr std::string_view FRAGMENT_INDEX = "fragment.index";

  EXTENSIONAPI static constexpr auto OutputFormat = core::PropertyDefinitionBuilder<magic_enum::enum_count<flow_file_source::OutputType>()>::createProperty("Output Format")
      .withDescription("Set the output format type.")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .withDefaultValue(magic_enum::enum_name(flow_file_source::OutputType::JSONPretty))
      .withAllowedValues(magic_enum::enum_names<flow_file_source::OutputType>())
      .build();
  EXTENSIONAPI static constexpr auto MaxRowsPerFlowFile = core::PropertyDefinitionBuilder<>::createProperty("Max Rows Per Flow File")
      .withDescription(
        "The maximum number of result rows that will be included in a single FlowFile. This will allow you to break up very large result sets into multiple FlowFiles. "
        "If the value specified is zero, then all rows are returned in a single FlowFile.")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("0")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({OutputFormat, MaxRowsPerFlowFile});

 protected:
  class FlowFileGenerator : public sql::SQLRowSubscriber {
   public:
    FlowFileGenerator(core::ProcessSession& session, sql::JSONSQLWriter& json_writer)
      : session_(session),
        json_writer_(json_writer) {}

    void beginProcessBatch() override {
      current_batch_size_ = 0;
    }
    void endProcessBatch() override;

    void finishProcessing() override;

    void beginProcessRow() override {}
    void endProcessRow() override {
      ++current_batch_size_;
    }
    void processColumnNames(const std::vector<std::string>& /*names*/) override {}
    void processColumn(const std::string& /*name*/, const std::string& /*value*/) override {}
    void processColumn(const std::string& /*name*/, double /*value*/) override {}
    void processColumn(const std::string& /*name*/, int /*value*/) override {}
    void processColumn(const std::string& /*name*/, long long /*value*/) override {}
    void processColumn(const std::string& /*name*/, unsigned long long /*value*/) override {}
    void processColumn(const std::string& /*name*/, const char* /*value*/) override {}

    std::shared_ptr<core::FlowFile> getLastFlowFile() const {
      if (!flow_files_.empty()) {
        return flow_files_.back();
      }
      return {};
    }

    std::vector<std::shared_ptr<core::FlowFile>>& getFlowFiles() {
      return flow_files_;
    }

   private:
    core::ProcessSession& session_;
    sql::JSONSQLWriter& json_writer_;
    const utils::Identifier batch_id_{utils::IdGenerator::getIdGenerator()->generate()};
    size_t current_batch_size_{0};
    std::vector<std::shared_ptr<core::FlowFile>> flow_files_;
  };

  flow_file_source::OutputType output_format_;
  size_t max_rows_{0};
};

}  // namespace org::apache::nifi::minifi::processors
