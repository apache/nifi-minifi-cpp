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

#include "core/logging/LoggerConfiguration.h"
#include "core/Processor.h"

#include "rapidjson/stream.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

#include "PerformanceDataCounter.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class PerformanceDataMonitor : public core::Processor {
 public:
  static constexpr const char* JSON_FORMAT_STR = "JSON";
  static constexpr const char* OPEN_TELEMETRY_FORMAT_STR = "OpenTelemetry";
  static constexpr const char* PRETTY_FORMAT_STR = "Pretty";
  static constexpr const char* COMPACT_FORMAT_STR = "Compact";

  explicit PerformanceDataMonitor(const std::string& name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid), output_format_(OutputFormat::JSON), pretty_output_(false),
        decimal_places_(std::nullopt), logger_(core::logging::LoggerFactory<PerformanceDataMonitor>::getLogger()),
        pdh_query_(nullptr), resource_consumption_counters_() {}

  ~PerformanceDataMonitor() override;

  EXTENSIONAPI static constexpr const char* Description = "This processor can create FlowFiles with various performance data through Performance Data Helper. (Windows only)";

  EXTENSIONAPI static const core::Property PredefinedGroups;
  EXTENSIONAPI static const core::Property CustomPDHCounters;
  EXTENSIONAPI static const core::Property OutputFormatProperty;
  EXTENSIONAPI static const core::Property OutputCompactness;
  EXTENSIONAPI static const core::Property DecimalPlaces;
  static auto properties() {
    return std::array{
      PredefinedGroups,
      CustomPDHCounters,
      OutputFormatProperty,
      OutputCompactness,
      DecimalPlaces
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;

 protected:
  enum class OutputFormat {
    JSON, OPENTELEMETRY
  };

  rapidjson::Value& prepareJSONBody(rapidjson::Document& root);

  void setupMembersFromProperties(const std::shared_ptr<core::ProcessContext>& context);
  void setupCountersFromProperties(const std::shared_ptr<core::ProcessContext>& context);
  void setupPredefinedGroupsFromProperties(const std::shared_ptr<core::ProcessContext>& context);
  void setupOutputFormatFromProperties(const std::shared_ptr<core::ProcessContext>& context);
  void setupDecimalPlacesFromProperties(const std::shared_ptr<core::ProcessContext>& context);
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
