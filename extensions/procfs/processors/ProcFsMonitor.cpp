/**
 * @file GenerateFlowFile.cpp
 * GenerateFlowFile class implementation
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

#include "ProcFsMonitor.h"
#include <limits>
#include <utility>
#include <vector>
#include <unordered_map>

#include "core/Resource.h"
#include "../ProcFsJsonSerialization.h"
#include "utils/JsonCallback.h"
#include "utils/OpenTelemetryLogDataModelUtils.h"
#include "utils/gsl.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::procfs {

const core::Relationship ProcFsMonitor::Success("success", "All files are routed to success");

const core::Property ProcFsMonitor::OutputFormatProperty(
    core::PropertyBuilder::createProperty("Output Format")->
        withDescription("The output type of the new flowfile")->
        withAllowableValues<std::string>(OutputFormat::values())->
        withDefaultValue(toString(OutputFormat::JSON))->build());

const core::Property ProcFsMonitor::OutputCompactnessProperty(
    core::PropertyBuilder::createProperty("Output Compactness")->
        withDescription("The output format of the new flowfile")->
        withAllowableValues<std::string>(OutputCompactness::values())->
        withDefaultValue(toString(OutputCompactness::PRETTY))->build());

const core::Property ProcFsMonitor::DecimalPlaces(
    core::PropertyBuilder::createProperty("Round to decimal places")->
        withDescription("The number of decimal places to round the values to (blank for no rounding)")->build());

const core::Property ProcFsMonitor::ResultRelativenessProperty(
    core::PropertyBuilder::createProperty("Result Type")->
        withDescription("Absolute returns the current procfs values, relative calculates the usage between triggers")->
        withAllowableValues<std::string>(ResultRelativeness::values())->
        withDefaultValue(toString(ResultRelativeness::RELATIVE))->build());


void ProcFsMonitor::initialize() {
  setSupportedProperties({OutputFormatProperty, OutputCompactnessProperty, DecimalPlaces, ResultRelativenessProperty});
  setSupportedRelationships({ProcFsMonitor::Success});
}

void ProcFsMonitor::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context);
  context->getProperty(OutputFormatProperty.getName(), output_format_);
  context->getProperty(OutputCompactnessProperty.getName(), output_compactness_);
  context->getProperty(ResultRelativenessProperty.getName(), result_relativeness_);
  setupDecimalPlacesFromProperties(*context);
}

namespace {
size_t number_of_cores(const std::unordered_map<std::string, CpuStatData>& cpu_stat) {
  return cpu_stat.size() > 1 ? cpu_stat.size() - 1 :  1;
}

bool cpu_stats_are_valid(const std::unordered_map<std::string, CpuStatData>& cpu_stat) {
  return cpu_stat.contains("cpu") && cpu_stat.size() > 1;  // needs the aggregate and at least one core information to be valid
}

std::optional<std::chrono::duration<double>> getAggregateCpuDiff(std::unordered_map<std::string, CpuStatData>& current_cpu_stats,
                                                                 std::unordered_map<std::string, CpuStatData>& last_cpu_stats) {
  if (current_cpu_stats.size() != last_cpu_stats.size())
    return std::nullopt;
  if (!cpu_stats_are_valid(current_cpu_stats) || !cpu_stats_are_valid(last_cpu_stats))
    return std::nullopt;

  return (current_cpu_stats.at("cpu").getTotal()-last_cpu_stats.at("cpu").getTotal()) / number_of_cores(current_cpu_stats);
}
}  // namespace

void ProcFsMonitor::onTrigger(core::ProcessContext*, core::ProcessSession* session) {
  gsl_Expects(session);
  std::shared_ptr<core::FlowFile> flowFile = session->create();
  if (!flowFile) {
    logger_->log_error("Failed to create flowfile!");
    yield();
    return;
  }

  rapidjson::Document root = rapidjson::Document(rapidjson::kObjectType);
  rapidjson::Value &body = prepareJSONBody(root);
  auto current_cpu_stats = proc_fs_.getCpuStats();
  auto current_disk_stats = proc_fs_.getDiskStats();
  auto current_net_devs = proc_fs_.getNetDevs();
  auto current_process_stats = proc_fs_.getProcessStats();

  auto last_cpu_period = getAggregateCpuDiff(current_cpu_stats, last_cpu_stats_);

  auto refresh_members = gsl::finally([&]{ refreshMembers(std::move(current_cpu_stats),
                                                          std::move(current_disk_stats),
                                                          std::move(current_net_devs),
                                                          std::move(current_process_stats));});

  processCPUInformation(current_cpu_stats, body, root.GetAllocator());
  processDiskInformation(current_disk_stats, body, root.GetAllocator());
  processNetworkInformation(current_net_devs, body, root.GetAllocator());
  processProcessInformation(current_process_stats, last_cpu_period, body, root.GetAllocator());
  processMemoryInformation(body, root.GetAllocator());

  if (output_compactness_ == OutputCompactness::PRETTY) {
    utils::PrettyJsonOutputCallback callback(std::move(root), decimal_places_);
    session->write(flowFile, &callback);
    session->transfer(flowFile, Success);
  } else if (output_compactness_ == OutputCompactness::COMPACT) {
    utils::JsonOutputCallback callback(std::move(root), decimal_places_);
    session->write(flowFile, &callback);
    session->transfer(flowFile, Success);
  } else {
    throw Exception(GENERAL_EXCEPTION, "Invalid output compactness");
  }
}

rapidjson::Value& ProcFsMonitor::prepareJSONBody(rapidjson::Document& root) {
  if (output_format_ == OutputFormat::OPENTELEMETRY) {
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

void ProcFsMonitor::setupDecimalPlacesFromProperties(const core::ProcessContext& context) {
  std::string decimal_places_str;
  if (!context.getProperty(DecimalPlaces.getName(), decimal_places_str) || decimal_places_str.empty()) {
    decimal_places_ = std::nullopt;
    return;
  }

  if (auto decimal_places = context.getProperty<int64_t>(DecimalPlaces)) {
    if (*decimal_places > std::numeric_limits<uint8_t>::max())
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "ProcFsMonitor Decimal Places is out of range");
    decimal_places_ = *decimal_places;
  }
  if (decimal_places_)
    logger_->log_trace("Rounding is enabled with %d decimal places", decimal_places_.value());
}

void ProcFsMonitor::processCPUInformation(const std::unordered_map<std::string, CpuStatData>& current_cpu_stats,
                                          rapidjson::Value& body,
                                          rapidjson::Document::AllocatorType& alloc) {
  if (!cpu_stats_are_valid(current_cpu_stats))
    return;

  rapidjson::Value cpu_root{rapidjson::kObjectType};
  for (auto& [cpu_name, cpu_stat] : current_cpu_stats) {
    if (result_relativeness_ == ResultRelativeness::ABSOLUTE) {
      addCPUStatToJson(cpu_name, cpu_stat, cpu_root, alloc);
    } else if (result_relativeness_ == ResultRelativeness::RELATIVE) {
      if (last_cpu_stats_.contains(cpu_name) && cpu_stat > last_cpu_stats_.at(cpu_name)) {
        addCPUStatPeriodToJson(cpu_name, cpu_stat, last_cpu_stats_.at(cpu_name), cpu_root, alloc);
      }
    } else {
      throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
    }
  }
  if (!cpu_root.ObjectEmpty())
    body.AddMember("CPU", cpu_root.Move(), alloc);
}

void ProcFsMonitor::processDiskInformation(const std::unordered_map<std::string, DiskStatData>& current_disk_stats,
                                           rapidjson::Value& body,
                                           rapidjson::Document::AllocatorType& alloc) {
  if (current_disk_stats.empty())
    return;

  rapidjson::Value disk_root{rapidjson::kObjectType};

  for (const auto& [disk_name, disk_stat] : current_disk_stats) {
    if (result_relativeness_ == ResultRelativeness::ABSOLUTE) {
      addDiskStatToJson(disk_name, disk_stat, disk_root, alloc);
    } else if (result_relativeness_ == ResultRelativeness::RELATIVE) {
      if (last_disk_stats_.contains(disk_name) && last_trigger) {
        addDiskStatPerSecToJson(disk_name, disk_stat-last_disk_stats_.at(disk_name), std::chrono::steady_clock::now()-*last_trigger, disk_root, alloc);
      }
    } else {
      throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
    }
  }
  if (!disk_root.ObjectEmpty())
    body.AddMember("Disk", disk_root.Move(), alloc);
}

void ProcFsMonitor::processNetworkInformation(const std::unordered_map<std::string, NetDevData>& current_net_devs,
                                              rapidjson::Value& body,
                                              rapidjson::Document::AllocatorType& alloc) {
  if (current_net_devs.empty())
    return;

  rapidjson::Value network_root{rapidjson::kObjectType};
  for (const auto& [net_dev_name, net_dev] : current_net_devs) {
    if (result_relativeness_ == ResultRelativeness::ABSOLUTE) {
      addNetDevToJson(net_dev_name, net_dev, network_root, alloc);
    } else if (result_relativeness_ == ResultRelativeness::RELATIVE) {
      if (last_net_devs_.contains(net_dev_name) && last_trigger) {
        addNetDevPerSecToJson(net_dev_name, net_dev-last_net_devs_.at(net_dev_name), std::chrono::steady_clock::now()-*last_trigger, network_root, alloc);
      }
    } else {
      throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
    }
  }
  if (!network_root.ObjectEmpty())
    body.AddMember("Network", network_root.Move(), alloc);
}

void ProcFsMonitor::processProcessInformation(const std::unordered_map<pid_t, ProcessStat>& current_process_stats,
                                              std::optional<std::chrono::duration<double>> last_cpu_period,
                                              rapidjson::Value& body,
                                              rapidjson::Document::AllocatorType& alloc) {
  if (current_process_stats.empty())
    return;

  rapidjson::Value process_root{rapidjson::kObjectType};
  for (const auto& [pid, process_stat] : current_process_stats) {
    if (result_relativeness_ == ResultRelativeness::ABSOLUTE) {
      addProcessStatToJson(std::to_string(pid), process_stat, process_root, alloc);
    } else if (result_relativeness_ == ResultRelativeness::RELATIVE) {
      if (!last_cpu_period || *last_cpu_period <= 0s)
        return;
      if (last_process_stats_.contains(pid) && last_process_stats_.at(pid).getComm() == process_stat.getComm()) {
        addNormalizedProcessStatToJson(std::to_string(pid), last_process_stats_.at(pid), process_stat, *last_cpu_period, process_root, alloc);
      }
    } else {
      throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
    }
  }
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


void ProcFsMonitor::refreshMembers(std::unordered_map<std::string, CpuStatData>&& current_cpu_stats,
                                   std::unordered_map<std::string, DiskStatData>&& current_disk_stats,
                                   std::unordered_map<std::string, NetDevData>&& current_net_devs,
                                   std::unordered_map<pid_t, ProcessStat>&& current_process_stats) {
  last_cpu_stats_ = std::move(current_cpu_stats);
  last_net_devs_ = std::move(current_net_devs);
  last_disk_stats_ = std::move(current_disk_stats);
  last_process_stats_ = std::move(current_process_stats);
  last_trigger = std::chrono::steady_clock::now();
}

REGISTER_RESOURCE(ProcFsMonitor, "This processor can create FlowFiles with various performance data through the proc pseudo-filesystem. (Linux only)");

}  // namespace org::apache::nifi::minifi::extensions::procfs
