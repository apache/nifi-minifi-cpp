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
#include <unordered_map>
#include <vector>
#include <optional>

#include "../ProcFs.h"
#include "core/Processor.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/logging/Logger.h"
#include "utils/Enum.h"

#include "rapidjson/stream.h"
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::extensions::procfs {

class ProcFsMonitor : public core::Processor {
 public:
  explicit ProcFsMonitor(const std::string& name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid) {
  }
  ProcFsMonitor(const ProcFsMonitor&) = delete;
  ProcFsMonitor(ProcFsMonitor&&) = delete;
  ProcFsMonitor& operator=(const ProcFsMonitor&) = delete;
  ProcFsMonitor& operator=(ProcFsMonitor&&) = delete;
  ~ProcFsMonitor() override = default;

  static constexpr char const *ProcessorName = "ProcFsMonitor";

  EXTENSIONAPI static const core::Property OutputFormatProperty;
  EXTENSIONAPI static const core::Property OutputCompactnessProperty;
  EXTENSIONAPI static const core::Property DecimalPlaces;
  EXTENSIONAPI static const core::Property ResultRelativenessProperty;

  EXTENSIONAPI static const core::Relationship Success;


 public:
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;

  void initialize() override;

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

 private:
  bool isSingleThreaded() const override {
    return true;
  }

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

 protected:
  rapidjson::Value& prepareJSONBody(rapidjson::Document& root);

  void setupDecimalPlacesFromProperties(const core::ProcessContext& context);

  void processCPUInformation(const std::unordered_map<std::string, CpuStatData>& current_cpu_stats,
                             rapidjson::Value& body,
                             rapidjson::Document::AllocatorType& alloc);
  void processDiskInformation(const std::unordered_map<std::string, DiskStatData>& current_disk_stats,
                              rapidjson::Value& body,
                              rapidjson::Document::AllocatorType& alloc);
  void processNetworkInformation(const std::unordered_map<std::string, NetDevData>& current_net_devs,
                                 rapidjson::Value& body,
                                 rapidjson::Document::AllocatorType& alloc);
  void processProcessInformation(const std::unordered_map<pid_t, ProcessStat>& current_process_stats,
                                 std::optional<std::chrono::duration<double>> last_cpu_period,
                                 rapidjson::Value& body,
                                 rapidjson::Document::AllocatorType& alloc);
  void processMemoryInformation(rapidjson::Value& body,
                                rapidjson::Document::AllocatorType& alloc);

  void refreshMembers(std::unordered_map<std::string, CpuStatData>&& current_cpu_stats,
                      std::unordered_map<std::string, DiskStatData>&& current_disk_stats,
                      std::unordered_map<std::string, NetDevData>&& current_net_devs,
                      std::unordered_map<pid_t, ProcessStat>&& current_process_stats);

  OutputFormat output_format_ = OutputFormat::JSON;
  OutputCompactness output_compactness_ = OutputCompactness::PRETTY;
  ResultRelativeness result_relativeness_ = ResultRelativeness::ABSOLUTE;

  std::optional<uint8_t> decimal_places_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ProcFsMonitor>::getLogger();

  ProcFs proc_fs_;

  std::unordered_map<std::string, CpuStatData> last_cpu_stats_;
  std::unordered_map<std::string, NetDevData> last_net_devs_;
  std::unordered_map<std::string, DiskStatData> last_disk_stats_;
  std::unordered_map<pid_t, ProcessStat> last_process_stats_;
  std::optional<std::chrono::steady_clock::time_point> last_trigger;
};

}  // namespace org::apache::nifi::minifi::extensions::procfs
