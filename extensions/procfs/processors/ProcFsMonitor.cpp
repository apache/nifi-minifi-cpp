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

#include "ProcFsMonitor.h"

#include <limits>
#include <utility>
#include <vector>
#include <unordered_map>

#include "core/Resource.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "../ProcFsJsonSerialization.h"
#include "utils/JsonCallback.h"
#include "utils/OpenTelemetryLogDataModelUtils.h"
#include "utils/gsl.h"
#include "utils/ProcessorConfigUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::procfs {

void ProcFsMonitor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ProcFsMonitor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  output_format_ = utils::parseEnumProperty<OutputFormat>(context, OutputFormatProperty);
  output_compactness_ = utils::parseEnumProperty<OutputCompactness>(context, OutputCompactnessProperty);
  result_relativeness_ = utils::parseEnumProperty<ResultRelativeness>(context, ResultRelativenessProperty);
  decimal_places_ = context.getProperty(DecimalPlaces) | utils::andThen(parsing::parseIntegral<uint8_t>) | utils::toOptional();
}

namespace {
size_t number_of_cores(const std::vector<std::pair<std::string, CpuStatData>>& cpu_stat) {
  gsl_Expects(cpu_stat.size() > 1);
  return cpu_stat.size() - 1;
}

bool cpu_stats_are_valid(const std::vector<std::pair<std::string, CpuStatData>>& cpu_stat) {
  return cpu_stat.size() > 1 && cpu_stat[0].first == "cpu";  // needs the aggregate and at least one core information to be valid
}

std::optional<std::chrono::duration<double>> getAggregateCpuDiff(std::vector<std::pair<std::string, CpuStatData>>& current_cpu_stats,
                                                                 std::vector<std::pair<std::string, CpuStatData>>& last_cpu_stats) {
  if (current_cpu_stats.size() != last_cpu_stats.size())
    return std::nullopt;
  if (!cpu_stats_are_valid(current_cpu_stats) || !cpu_stats_are_valid(last_cpu_stats))
    return std::nullopt;

  return (current_cpu_stats[0].second.getTotal() - last_cpu_stats[0].second.getTotal()) / number_of_cores(current_cpu_stats);
}
}  // namespace

void ProcFsMonitor::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  std::shared_ptr<core::FlowFile> flowFile = session.create();

  rapidjson::Document root = rapidjson::Document(rapidjson::kObjectType);
  rapidjson::Value& body = prepareJSONBody(root);
  auto current_cpu_stats = proc_fs_.getCpuStats();
  auto current_disk_stats = proc_fs_.getDiskStats();
  auto current_net_devs = proc_fs_.getNetDevs();
  auto current_process_stats = proc_fs_.getProcessStats();

  auto last_cpu_period = getAggregateCpuDiff(current_cpu_stats, last_cpu_stats_);

  auto refresh_members = gsl::finally([&] {
    refreshMembers(std::move(current_cpu_stats),
                   std::move(current_disk_stats),
                   std::move(current_net_devs),
                   std::move(current_process_stats));
  });

  processCPUInformation(current_cpu_stats, body, root.GetAllocator());
  processDiskInformation(current_disk_stats, body, root.GetAllocator());
  processNetworkInformation(current_net_devs, body, root.GetAllocator());
  processProcessInformation(current_process_stats, last_cpu_period, body, root.GetAllocator());
  processMemoryInformation(body, root.GetAllocator());

  if (output_compactness_ == OutputCompactness::Pretty) {
    utils::PrettyJsonOutputCallback callback(std::move(root), decimal_places_);
    session.write(flowFile, std::ref(callback));
    session.transfer(flowFile, Success);
  } else if (output_compactness_ == OutputCompactness::Compact) {
    utils::JsonOutputCallback callback(std::move(root), decimal_places_);
    session.write(flowFile, std::ref(callback));
    session.transfer(flowFile, Success);
  } else {
    throw Exception(GENERAL_EXCEPTION, "Invalid output compactness");
  }
}

rapidjson::Value& ProcFsMonitor::prepareJSONBody(rapidjson::Document& root) {
  if (output_format_ == OutputFormat::OpenTelemetry) {
    utils::OpenTelemetryLogDataModel::appendEventInformation(root, "PerformanceData");
    utils::OpenTelemetryLogDataModel::appendHostInformation(root);
    utils::OpenTelemetryLogDataModel::appendBody(root);
    return root["Body"];
  } else if (output_format_ == OutputFormat::JSON) {
    return root;
  } else {
    throw Exception(GENERAL_EXCEPTION, "Invalid output format");
  }
}

namespace {
void processAbsoluteCPUInformation(const std::vector<std::pair<std::string, CpuStatData>>& current_cpu_stats,
                                   rapidjson::Value& body,
                                   rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value cpu_root{rapidjson::kObjectType};
  for (const auto& [cpu_name, cpu_stat] : current_cpu_stats)
    addCPUStatToJson(cpu_name, cpu_stat, cpu_root, alloc);
  if (!cpu_root.ObjectEmpty())
    body.AddMember("CPU", cpu_root.Move(), alloc);
}

void processRelativeCPUInformation(const std::vector<std::pair<std::string, CpuStatData>>& current_cpu_stats,
                                   const std::vector<std::pair<std::string, CpuStatData>>& last_cpu_stats,
                                   rapidjson::Value& body,
                                   rapidjson::Document::AllocatorType& alloc) {
  if (last_cpu_stats.size() != current_cpu_stats.size())
    return;

  rapidjson::Value cpu_root{rapidjson::kObjectType};
  for (size_t i = 0; i < current_cpu_stats.size(); ++i) {
    const auto& [cpu_name, cpu_stat] = current_cpu_stats[i];
    const auto& [last_cpu_name, last_cpu_stat] = last_cpu_stats[i];
    gsl_Expects(last_cpu_name == cpu_name);
    if (cpu_stat.getTotal() > last_cpu_stat.getTotal())
      addCPUStatPeriodToJson(cpu_name, cpu_stat, last_cpu_stat, cpu_root, alloc);
  }
  if (!cpu_root.ObjectEmpty())
    body.AddMember("CPU", cpu_root.Move(), alloc);
}

void processAbsoluteNetworkInformation(const std::vector<std::pair<std::string, NetDevData>>& current_net_devs,
                                       rapidjson::Value& body,
                                       rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value network_root{rapidjson::kObjectType};
  for (const auto& [interface_name, net_dev] : current_net_devs)
    addNetDevToJson(interface_name, net_dev, network_root, alloc);
  if (!network_root.ObjectEmpty())
    body.AddMember("Network", network_root.Move(), alloc);
}

void processRelativeNetworkInformation(const std::vector<std::pair<std::string, NetDevData>>& current_net_devs,
                                       const std::vector<std::pair<std::string, NetDevData>>& last_net_devs,
                                       const std::optional<std::chrono::steady_clock::time_point> last_trigger_,
                                       rapidjson::Value& body,
                                       rapidjson::Document::AllocatorType& alloc) {
  if (!last_trigger_)
    return;
  std::chrono::duration<double> duration = std::chrono::steady_clock::now() - *last_trigger_;
  if (duration <= 0ms)
    return;

  rapidjson::Value network_root{rapidjson::kObjectType};
  for (const auto& current_net_dev_it : current_net_devs) {
    auto& interface_name = current_net_dev_it.first;
    auto& current_net_dev = current_net_dev_it.second;
    auto last_net_dev_it = std::find_if(last_net_devs.begin(), last_net_devs.end(), [&interface_name](auto& last_net_dev) { return last_net_dev.first == interface_name; });
    if (last_net_dev_it == last_net_devs.end())
      continue;
    auto& last_net_dev = last_net_dev_it->second;
    addNetDevPerSecToJson(interface_name, current_net_dev - last_net_dev, duration, network_root, alloc);
  }
  if (!network_root.ObjectEmpty())
    body.AddMember("Network", network_root.Move(), alloc);
}

void processAbsoluteDiskInformation(const std::vector<std::pair<std::string, DiskStatData>>& current_disk_stats,
                                    rapidjson::Value& body,
                                    rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value disk_root{rapidjson::kObjectType};
  for (const auto& [disk_name, disk_stat] : current_disk_stats)
    addDiskStatToJson(disk_name, disk_stat, disk_root, alloc);
  if (!disk_root.ObjectEmpty())
    body.AddMember("Disk", disk_root.Move(), alloc);
}

void processRelativeDiskInformation(const std::vector<std::pair<std::string, DiskStatData>>& current_disk_stats,
                                    const std::vector<std::pair<std::string, DiskStatData>>& last_disk_stats,
                                    const std::optional<std::chrono::steady_clock::time_point> last_trigger_,
                                    rapidjson::Value& body,
                                    rapidjson::Document::AllocatorType& alloc) {
  if (!last_trigger_)
    return;
  std::chrono::duration<double> duration = std::chrono::steady_clock::now() - *last_trigger_;
  if (duration <= 0ms)
    return;

  rapidjson::Value disk_root{rapidjson::kObjectType};
  for (const auto& current_disk_stat_it : current_disk_stats) {
    auto& disk_name = current_disk_stat_it.first;
    auto& current_disk_stat = current_disk_stat_it.second;
    auto last_disk_stat_it = std::find_if(last_disk_stats.begin(), last_disk_stats.end(), [&disk_name](auto& last_disk_stat) { return last_disk_stat.first == disk_name; });
    if (last_disk_stat_it == last_disk_stats.end())
      continue;
    auto& last_disk_stat = last_disk_stat_it->second;
    addDiskStatPerSecToJson(disk_name, current_disk_stat - last_disk_stat, duration, disk_root, alloc);
  }
  if (!disk_root.ObjectEmpty())
    body.AddMember("Disk", disk_root.Move(), alloc);
}

void processAbsoluteProcessInformation(const std::map<pid_t, ProcessStat>& current_process_stats,
                                       rapidjson::Value& body,
                                       rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value process_root{rapidjson::kObjectType};
  for (const auto&[pid, process_stat] : current_process_stats) {
    addProcessStatToJson(std::to_string(pid), process_stat, process_root, alloc);
  }
  if (!process_root.ObjectEmpty())
    body.AddMember("Process", process_root.Move(), alloc);
}

void processRelativeProcessInformation(const std::map<pid_t, ProcessStat>& current_process_stats,
                                       const std::map<pid_t, ProcessStat>& last_process_stats,
                                       std::optional<std::chrono::duration<double>> last_cpu_period,
                                       rapidjson::Value& body,
                                       rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value process_root{rapidjson::kObjectType};
  if (!last_cpu_period || *last_cpu_period <= 0s)
    return;

  for (const auto& [pid, process_stat] : current_process_stats) {
    if (last_process_stats.contains(pid) && last_process_stats.at(pid).getComm() == process_stat.getComm()) {
      addNormalizedProcessStatToJson(std::to_string(pid), last_process_stats.at(pid), process_stat, *last_cpu_period, process_root, alloc);
    }
  }
  if (!process_root.ObjectEmpty())
    body.AddMember("Process", process_root.Move(), alloc);
}
}  // namespace

void ProcFsMonitor::processCPUInformation(const std::vector<std::pair<std::string, CpuStatData>>& current_cpu_stats,
                                          rapidjson::Value& body,
                                          rapidjson::Document::AllocatorType& alloc) {
  if (!cpu_stats_are_valid(current_cpu_stats))
    return;

  if (result_relativeness_ == ResultRelativeness::Relative)
    processRelativeCPUInformation(current_cpu_stats, last_cpu_stats_, body, alloc);
  else if (result_relativeness_ == ResultRelativeness::Absolute)
    processAbsoluteCPUInformation(current_cpu_stats, body, alloc);
  else
    throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
}

void ProcFsMonitor::processDiskInformation(const std::vector<std::pair<std::string, DiskStatData>>& current_disk_stats,
                                           rapidjson::Value& body,
                                           rapidjson::Document::AllocatorType& alloc) {
  if (current_disk_stats.empty())
    return;

  if (result_relativeness_ == ResultRelativeness::Relative)
    processRelativeDiskInformation(current_disk_stats, last_disk_stats_, last_trigger_, body, alloc);
  else if (result_relativeness_ == ResultRelativeness::Absolute)
    processAbsoluteDiskInformation(current_disk_stats, body, alloc);
  else
    throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
}

void ProcFsMonitor::processNetworkInformation(const std::vector<std::pair<std::string, NetDevData>>& current_net_devs,
                                              rapidjson::Value& body,
                                              rapidjson::Document::AllocatorType& alloc) {
  if (current_net_devs.empty())
    return;

  if (result_relativeness_ == ResultRelativeness::Relative)
    processRelativeNetworkInformation(current_net_devs, last_net_devs_, last_trigger_, body, alloc);
  else if (result_relativeness_ == ResultRelativeness::Absolute)
    processAbsoluteNetworkInformation(current_net_devs, body, alloc);
  else
    throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
}

void ProcFsMonitor::processProcessInformation(const std::map<pid_t, ProcessStat>& current_process_stats,
                                              std::optional<std::chrono::duration<double>> last_cpu_period,
                                              rapidjson::Value& body,
                                              rapidjson::Document::AllocatorType& alloc) {
  if (current_process_stats.empty())
    return;

  rapidjson::Value process_root{rapidjson::kObjectType};
  if (result_relativeness_ == ResultRelativeness::Relative)
    processRelativeProcessInformation(current_process_stats, last_process_stats_, last_cpu_period, body, alloc);
  else if (result_relativeness_ == ResultRelativeness::Absolute)
    processAbsoluteProcessInformation(current_process_stats, body, alloc);
  else
    throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
  if (!process_root.ObjectEmpty())
    body.AddMember("Process", process_root.Move(), alloc);
}

void ProcFsMonitor::processMemoryInformation(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) {
  if (auto mem_info = proc_fs_.getMemInfo()) {
    body.AddMember("Memory", rapidjson::Value{rapidjson::kObjectType}, alloc);
    auto& memory_root = body["Memory"];
    addMemInfoToJson(*mem_info, memory_root, alloc);
  }
}


void ProcFsMonitor::refreshMembers(std::vector<std::pair<std::string, CpuStatData>>&& current_cpu_stats,
                                   std::vector<std::pair<std::string, DiskStatData>>&& current_disk_stats,
                                   std::vector<std::pair<std::string, NetDevData>>&& current_net_devs,
                                   std::map<pid_t, ProcessStat>&& current_process_stats) {
  last_cpu_stats_ = std::move(current_cpu_stats);
  last_net_devs_ = std::move(current_net_devs);
  last_disk_stats_ = std::move(current_disk_stats);
  last_process_stats_ = std::move(current_process_stats);
  last_trigger_ = std::chrono::steady_clock::now();
}

REGISTER_RESOURCE(ProcFsMonitor, Processor);

}  // namespace org::apache::nifi::minifi::extensions::procfs
