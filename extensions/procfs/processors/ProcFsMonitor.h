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
#include <memory>
#include <map>
#include <vector>
#include <utility>
#include <optional>

#include "../ProcFs.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/logging/Logger.h"
#include "utils/Enum.h"

#include "rapidjson/stream.h"
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::extensions::procfs {

SMART_ENUM(OutputFormat,
    (JSON, "JSON"),
    (OPENTELEMETRY, "OpenTelemetry")
)

SMART_ENUM(OutputCompactness,
    (COMPACT, "Compact"),
    (PRETTY, "Pretty")
)

SMART_ENUM(ResultRelativeness,
    (RELATIVE, "Relative"),
    (ABSOLUTE, "Absolute")
)

class ProcFsMonitor : public core::Processor {
 public:
  explicit ProcFsMonitor(std::string name, utils::Identifier uuid = utils::Identifier())
      : Processor(std::move(name), uuid) {
  }
  ~ProcFsMonitor() override = default;

  EXTENSIONAPI static constexpr const char* Description = "This processor can create FlowFiles with various performance data through the proc pseudo-filesystem. (Linux only)";

  EXTENSIONAPI static constexpr auto OutputFormatProperty = core::PropertyDefinitionBuilder<OutputFormat::length>::createProperty("Output Format")
      .withDescription("The output type of the new flowfile")
      .withAllowedValues(OutputFormat::values)
      .withDefaultValue(toStringView(OutputFormat::JSON))
      .build();
  EXTENSIONAPI static constexpr auto OutputCompactnessProperty = core::PropertyDefinitionBuilder<OutputCompactness::length>::createProperty("Output Compactness")
      .withDescription("The output format of the new flowfile")
      .withAllowedValues(OutputCompactness::values)
      .withDefaultValue(toStringView(OutputCompactness::PRETTY))
      .build();
  EXTENSIONAPI static constexpr auto DecimalPlaces = core::PropertyDefinitionBuilder<>::createProperty("Round to decimal places")
      .withDescription("The number of decimal places to round the values to (blank for no rounding)")
      .build();
  EXTENSIONAPI static constexpr auto ResultRelativenessProperty = core::PropertyDefinitionBuilder<ResultRelativeness::length>::createProperty("Result Type")
      .withDescription("Absolute returns the current procfs values, relative calculates the usage between triggers")
      .withAllowedValues(ResultRelativeness::values)
      .withDefaultValue(toStringView(ResultRelativeness::ABSOLUTE))
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 4>{
      OutputFormatProperty,
      OutputCompactnessProperty,
      DecimalPlaces,
      ResultRelativenessProperty
  };


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;

  void initialize() override;

 private:
  rapidjson::Value& prepareJSONBody(rapidjson::Document& root);

  void setupDecimalPlacesFromProperties(const core::ProcessContext& context);

  void processCPUInformation(const std::vector<std::pair<std::string, CpuStatData>>& current_cpu_stats,
                             rapidjson::Value& body,
                             rapidjson::Document::AllocatorType& alloc);

  void processDiskInformation(const std::vector<std::pair<std::string, DiskStatData>>& current_disk_stats,
                              rapidjson::Value& body,
                              rapidjson::Document::AllocatorType& alloc);

  void processNetworkInformation(const std::vector<std::pair<std::string, NetDevData>>& current_net_devs,
                                 rapidjson::Value& body,
                                 rapidjson::Document::AllocatorType& alloc);

  void processProcessInformation(const std::map<pid_t, ProcessStat>& current_process_stats,
                                 std::optional<std::chrono::duration<double>> last_cpu_period,
                                 rapidjson::Value& body,
                                 rapidjson::Document::AllocatorType& alloc);

  void processMemoryInformation(rapidjson::Value& body,
                                rapidjson::Document::AllocatorType& alloc);

  void refreshMembers(std::vector<std::pair<std::string, CpuStatData>>&& current_cpu_stats,
                      std::vector<std::pair<std::string, DiskStatData>>&& current_disk_stats,
                      std::vector<std::pair<std::string, NetDevData>>&& current_net_devs,
                      std::map<pid_t, ProcessStat>&& current_process_stats);

  OutputFormat output_format_ = OutputFormat::JSON;
  OutputCompactness output_compactness_ = OutputCompactness::PRETTY;
  ResultRelativeness result_relativeness_ = ResultRelativeness::ABSOLUTE;

  std::optional<uint8_t> decimal_places_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ProcFsMonitor>::getLogger(uuid_);

  ProcFs proc_fs_;

  std::vector<std::pair<std::string, CpuStatData>> last_cpu_stats_;
  std::vector<std::pair<std::string, NetDevData>> last_net_devs_;
  std::vector<std::pair<std::string, DiskStatData>> last_disk_stats_;
  std::map<pid_t, ProcessStat> last_process_stats_;
  std::optional<std::chrono::steady_clock::time_point> last_trigger_;
};

}  // namespace org::apache::nifi::minifi::extensions::procfs
