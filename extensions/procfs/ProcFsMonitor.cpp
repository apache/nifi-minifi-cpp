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
#include "utils/JsonCallback.h"
#include "utils/OpenTelemetryLogDataModelUtils.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::procfs::processors {

const core::Relationship ProcFsMonitor::Success("success", "All files are routed to success");

const core::Property ProcFsMonitor::OutputFormatProperty(
    core::PropertyBuilder::createProperty("Output Format")->
        withDescription("Format of the created flowfiles")->
        withAllowableValues<std::string>(OutputFormat::values())->
        withDefaultValue(toString(OutputFormat::JSON))->build());

const core::Property ProcFsMonitor::OutputCompactnessProperty(
    core::PropertyBuilder::createProperty("Output Compactness")->
        withDescription("Format of the created flowfiles")->
        withAllowableValues<std::string>(OutputCompactness::values())->
        withDefaultValue(toString(OutputCompactness::PRETTY))->build());

const core::Property ProcFsMonitor::DecimalPlaces(
    core::PropertyBuilder::createProperty("Round to decimal places")->
        withDescription("The number of decimal places to round the values to (blank for no rounding)")->build());

const core::Property ProcFsMonitor::ResultRelativenessProperty(
    core::PropertyBuilder::createProperty("ResultRelativeness")->
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
  processMemoryInformation(body, root.GetAllocator());
  processNetworkInformation(body, root.GetAllocator());
  processDiskInformation(body, root.GetAllocator());
  processCPUInformation(body, root.GetAllocator());
  processProcessInformation(body, root.GetAllocator());

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

  int64_t decimal_places;
  if (context.getProperty(DecimalPlaces.getName(), decimal_places)) {
    if (decimal_places > std::numeric_limits<uint8_t>::max() || decimal_places < 1)
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "ProcFsMonitor Decimal Places is out of range");
    decimal_places_ = static_cast<uint8_t>(decimal_places);
  }
  if (decimal_places_.has_value())
    logger_->log_trace("Rounding is enabled with %d decimal places", decimal_places_.value());
}

void ProcFsMonitor::processMemoryInformation(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) {
  auto mem_info = proc_fs_.getMemInfo();
  if (mem_info.has_value()) {
    body.AddMember("Memory", rapidjson::Value{rapidjson::kObjectType}, alloc);
    mem_info.value().addToJson(body["Memory"], alloc);
  }
}

void ProcFsMonitor::processNetworkInformation(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) {
  std::vector<NetDev> net_devs = proc_fs_.getNetDevs();
  if (net_devs.empty())
    return;
  body.AddMember("Network", rapidjson::Value{rapidjson::kObjectType}, alloc);
  rapidjson::Value& network_root = body["Network"];
  auto refresh_last_net_devs = gsl::finally([this, &net_devs]{last_net_devs_ = std::move(net_devs);});
  for (const auto& net_dev : net_devs) {
    if (result_relativeness_ == ResultRelativeness::ABSOLUTE) {
      net_dev.addToJson(network_root, alloc);
    } else if (result_relativeness_ == ResultRelativeness::RELATIVE) {
      const std::string& interface_name = net_dev.getInterfaceName();
      auto last_net_dev = std::find_if(last_net_devs_.begin(), last_net_devs_.end(), [&interface_name](const NetDev& net_dev) -> bool { return net_dev.getInterfaceName() == interface_name; });
      if (last_net_dev != last_net_devs_.end() && net_dev.getData() >= last_net_dev->getData()) {
        auto net_dev_period = NetDevPeriod::create(*last_net_dev, net_dev);
        if (net_dev_period.has_value())
          net_dev_period.value().addToJson(network_root, alloc);
      }
    } else {
      throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
    }
  }
}

void ProcFsMonitor::processDiskInformation(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) {
  std::vector<DiskStat> disk_stats = proc_fs_.getDiskStats();
  if (disk_stats.empty())
    return;

  body.AddMember("Disk", rapidjson::Value{rapidjson::kObjectType}, alloc);
  rapidjson::Value& disk_root = body["Disk"];
  auto refresh_last_disk_stats = gsl::finally([this, &disk_stats]{last_disk_stats_ = std::move(disk_stats);});
  for (const auto& disk_stat : disk_stats) {
    if (result_relativeness_ == ResultRelativeness::ABSOLUTE) {
      disk_stat.addToJson(disk_root, alloc);
    } else if (result_relativeness_ == ResultRelativeness::RELATIVE) {
      const std::string& disk_name = disk_stat.getData().getDiskName();
      auto last_disk_stat = std::find_if(last_disk_stats_.begin(),
                                         last_disk_stats_.end(),
                                         [&disk_name](const DiskStat& disk_stat) -> bool { return disk_stat.getData().getDiskName() == disk_name; });
      if (last_disk_stat != last_disk_stats_.end()) {
        auto disk_stat_period = DiskStatPeriod::create(*last_disk_stat, disk_stat);
        if (disk_stat_period.has_value())
          disk_stat_period.value().addToJson(disk_root, alloc);
      }
    } else {
      throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
    }
  }
}

namespace {
size_t number_of_cores(const std::vector<CpuStat>& cpu_stats) {
  return cpu_stats.size() > 1 ? cpu_stats.size() - 1 :  0;
}
}  // namespace

void ProcFsMonitor::refreshLastCpuStats(std::vector<CpuStat>&& current_cpu_stats) {
  last_cpu_period_ = std::nullopt;
  if (current_cpu_stats.size() == last_cpu_stats_.size() && current_cpu_stats.size() > 1) {
    CpuStat& current_aggregate_cpu_stat = current_cpu_stats[0];
    CpuStat& last_aggregate_cpu_stat = last_cpu_stats_[0];
    if (current_aggregate_cpu_stat.getData() >= last_aggregate_cpu_stat.getData()) {
      auto aggregate_cpu_stat_period = CpuStatPeriod::create(last_aggregate_cpu_stat, current_aggregate_cpu_stat);
      if (aggregate_cpu_stat_period.has_value())
        last_cpu_period_ = aggregate_cpu_stat_period.value().getPeriod() / number_of_cores(current_cpu_stats);
    }
  }
  last_cpu_stats_ = std::move(current_cpu_stats);
}

namespace {
bool cpu_stats_are_valid(const std::vector<CpuStat>& cpu_stat) {
  return cpu_stat.size() > 1;  // needs the aggregate and at least one core information to be valid
}
}

void ProcFsMonitor::processCPUInformation(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) {
  std::vector<CpuStat> cpu_stats = proc_fs_.getCpuStats();
  if (!cpu_stats_are_valid(cpu_stats))
    return;
  auto refresh_last_cpu_stats = gsl::finally([this, &cpu_stats]{
    this->refreshLastCpuStats(std::move(cpu_stats));
  });

  body.AddMember("CPU", rapidjson::Value{rapidjson::kObjectType}, alloc);
  rapidjson::Value& cpu_root = body["CPU"];
  for (uint32_t i = 0; i < cpu_stats.size(); ++i) {
    if (result_relativeness_ == ResultRelativeness::ABSOLUTE) {
      cpu_stats[i].addToJson(cpu_root, alloc);
    } else if (result_relativeness_ == ResultRelativeness::RELATIVE) {
      if (cpu_stats.size() == last_cpu_stats_.size() && (cpu_stats[i].getData() >= last_cpu_stats_[i].getData())) {
        auto cpu_stat_period = CpuStatPeriod::create(last_cpu_stats_[i], cpu_stats[i]);
        if (cpu_stat_period.has_value()) {
          cpu_stat_period->addToJson(cpu_root, alloc);
        }
      }
    } else {
      throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
    }
  }
}

void ProcFsMonitor::processProcessInformation(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) {
  std::unordered_map<pid_t, ProcessStat> process_stats = proc_fs_.getProcessStats();
  if (process_stats.empty())
    return;

  body.AddMember("Process", rapidjson::Value{rapidjson::kObjectType}, alloc);
  rapidjson::Value& process_root = body["Process"];
  auto refresh_last_process_stats = gsl::finally([this, &process_stats]{last_process_stats_ = std::move(process_stats);});
  for (const auto& process_stat : process_stats) {
    if (result_relativeness_ == ResultRelativeness::ABSOLUTE) {
      process_stat.second.addToJson(process_root, alloc);
    } else if (result_relativeness_ == ResultRelativeness::RELATIVE) {
      if (!last_cpu_period_.has_value() || last_cpu_period_.value() <= 0)
        return;
      auto last_stat_it = last_process_stats_.find(process_stat.first);
      if (last_stat_it != last_process_stats_.end()) {
        auto process_stat_period = ProcessStatPeriod::create(last_stat_it->second, process_stat.second, last_cpu_period_.value());
        if (process_stat_period.has_value()) {
          process_stat_period->addToJson(process_root, alloc);
        }
      }
    } else {
      throw Exception(GENERAL_EXCEPTION, "Invalid result relativeness");
    }
  }
}

REGISTER_RESOURCE(ProcFsMonitor, "This processor can create FlowFiles with various performance data through Performance Data Helper. (Windows only)");

}  // namespace org::apache::nifi::minifi::procfs::processors
