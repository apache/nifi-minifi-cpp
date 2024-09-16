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

#include <pdh.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/logging/LoggerFactory.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"

#include "rapidjson/stream.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

#include "PerformanceDataCounter.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class PerformanceDataMonitor final : public core::ProcessorImpl {
 public:
  static constexpr const char* JSON_FORMAT_STR = "JSON";
  static constexpr const char* OPEN_TELEMETRY_FORMAT_STR = "OpenTelemetry";
  static constexpr const char* PRETTY_FORMAT_STR = "Pretty";
  static constexpr const char* COMPACT_FORMAT_STR = "Compact";

  explicit PerformanceDataMonitor(const std::string_view name, utils::Identifier uuid = utils::Identifier())
      : ProcessorImpl(name, uuid), output_format_(OutputFormat::JSON), pretty_output_(false),
        decimal_places_(std::nullopt), logger_(core::logging::LoggerFactory<PerformanceDataMonitor>::getLogger()),
        pdh_query_(nullptr), resource_consumption_counters_() {}

  ~PerformanceDataMonitor() override;

  EXTENSIONAPI static constexpr const char* Description =
      "This Windows only processor can create FlowFiles populated with various performance data with the help of Windows Performance Counters. "
      "Windows Performance Counters provide a high-level abstraction layer with a consistent interface for collecting various kinds of system data such as CPU, memory, and disk usage statistics.";

  EXTENSIONAPI static constexpr auto PredefinedGroups = core::PropertyDefinitionBuilder<>::createProperty("Predefined Groups")
      .withDescription(R"(Comma separated list from the allowable values, to monitor multiple common Windows Performance counters related to these groups (e.g. "CPU,Network"))")
      .withDefaultValue("")
      .build();
  EXTENSIONAPI static constexpr auto CustomPDHCounters = core::PropertyDefinitionBuilder<>::createProperty("Custom PDH Counters")
      .withDescription(R"(Comma separated list of Windows Performance Counters to monitor (e.g. "\\System\\Threads,\\Process(*)\\ID Process"))")
      .withDefaultValue("")
      .build();
  EXTENSIONAPI static constexpr auto OutputFormatProperty = core::PropertyDefinitionBuilder<2>::createProperty("Output Format")
      .withDescription("Format of the created flowfiles")
      .withAllowedValues({ JSON_FORMAT_STR, OPEN_TELEMETRY_FORMAT_STR })
      .withDefaultValue(JSON_FORMAT_STR)
      .build();
  EXTENSIONAPI static constexpr auto OutputCompactness = core::PropertyDefinitionBuilder<2>::createProperty("Output Compactness")
      .withDescription("Format of the created flowfiles")
      .withAllowedValues({ PRETTY_FORMAT_STR, COMPACT_FORMAT_STR})
      .withDefaultValue(PRETTY_FORMAT_STR)
      .build();
  EXTENSIONAPI static constexpr auto DecimalPlaces = core::PropertyDefinitionBuilder<>::createProperty("Round to decimal places")
      .withDescription("The number of decimal places to round the values to (blank for no rounding)")
      .withDefaultValue("")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      PredefinedGroups,
      CustomPDHCounters,
      OutputFormatProperty,
      OutputCompactness,
      DecimalPlaces
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 protected:
  enum class OutputFormat {
    JSON, OPENTELEMETRY
  };

  rapidjson::Value& prepareJSONBody(rapidjson::Document& root);

  void setupMembersFromProperties(core::ProcessContext& context);
  void setupCountersFromProperties(core::ProcessContext& context);
  void setupPredefinedGroupsFromProperties(core::ProcessContext& context);
  void setupOutputFormatFromProperties(core::ProcessContext& context);
  void setupDecimalPlacesFromProperties(core::ProcessContext& context);
  void addCountersFromPredefinedGroupsProperty(const std::string& custom_pdh_counters);
  void addCustomPDHCountersFromProperty(const std::string& custom_pdh_counters);

  OutputFormat output_format_;
  bool pretty_output_;

  std::optional<uint8_t> decimal_places_;
  std::shared_ptr<core::logging::Logger> logger_;
  PDH_HQUERY pdh_query_;
  std::vector<std::unique_ptr<PerformanceDataCounter>> resource_consumption_counters_;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
