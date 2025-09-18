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

#include "PerformanceDataMonitor.h"
#include <limits>

#include "PDHCounters.h"
#include "MemoryConsumptionCounter.h"
#include "utils/StringUtils.h"
#include "utils/JsonCallback.h"
#include "utils/OpenTelemetryLogDataModelUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

PerformanceDataMonitor::~PerformanceDataMonitor() {
  PdhCloseQuery(pdh_query_);
}

void PerformanceDataMonitor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  setupMembersFromProperties(context);

  PdhOpenQueryA(nullptr, 0, &pdh_query_);

  for (auto it = resource_consumption_counters_.begin(); it != resource_consumption_counters_.end();) {
    PDHCounter* pdh_counter = dynamic_cast<PDHCounter*> (it->get());
    if (pdh_counter != nullptr) {
      PDH_STATUS add_to_query_result = pdh_counter->addToQuery(pdh_query_);
      if (add_to_query_result != ERROR_SUCCESS) {
        logger_->log_error("Error adding {} to query, error code: {:#x}", pdh_counter->getName(), add_to_query_result);
        it = resource_consumption_counters_.erase(it);
        continue;
      }
    }
    ++it;
  }

  PDH_STATUS collect_query_data_result = PdhCollectQueryData(pdh_query_);
  if (ERROR_SUCCESS != collect_query_data_result) {
    logger_->log_error("Error during PdhCollectQueryData, error code: {:#x}", collect_query_data_result);
  }
}

void PerformanceDataMonitor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  if (resource_consumption_counters_.empty()) {
    logger_->log_error("No valid counters for PerformanceDataMonitor");
    context.yield();
    return;
  }

  std::shared_ptr<core::FlowFile> flowFile = session.create();
  if (!flowFile) {
    logger_->log_error("Failed to create flowfile!");
    context.yield();
    return;
  }

  PDH_STATUS collect_query_data_result = PdhCollectQueryData(pdh_query_);
  if (ERROR_SUCCESS != collect_query_data_result) {
    logger_->log_error("Error during PdhCollectQueryData, error code: {:#x}", collect_query_data_result);
    context.yield();
    return;
  }

  rapidjson::Document root = rapidjson::Document(rapidjson::kObjectType);
  rapidjson::Value& body = prepareJSONBody(root);
  for (auto& counter : resource_consumption_counters_) {
    if (counter->collectData()) {
      counter->addToJson(body, root.GetAllocator());
    }
  }
  if (pretty_output_) {
    utils::PrettyJsonOutputCallback callback(std::move(root), decimal_places_);
    session.write(flowFile, std::ref(callback));
    session.transfer(flowFile, Success);
  } else {
    utils::JsonOutputCallback callback(std::move(root), decimal_places_);
    session.write(flowFile, std::ref(callback));
    session.transfer(flowFile, Success);
  }
}

void PerformanceDataMonitor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

rapidjson::Value& PerformanceDataMonitor::prepareJSONBody(rapidjson::Document& root) {
  switch (output_format_) {
    case OutputFormat::OPENTELEMETRY: {
      utils::OpenTelemetryLogDataModel::appendEventInformation(root, "PerformanceData");
      utils::OpenTelemetryLogDataModel::appendHostInformation(root);
      utils::OpenTelemetryLogDataModel::appendBody(root);
      return root["Body"];
    }
    case OutputFormat::JSON:
      return root;
    default:
      return root;
  }
}

void add_cpu_related_counters(std::vector<std::unique_ptr<PerformanceDataCounter>>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Processor(*)\\% Processor Time"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Processor(*)\\% User Time"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Processor(*)\\% Privileged Time"));
}

void add_io_related_counters(std::vector<std::unique_ptr<PerformanceDataCounter>>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Process(_Total)\\IO Read Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Process(_Total)\\IO Write Bytes/sec"));
}

void add_disk_related_counters(std::vector<std::unique_ptr<PerformanceDataCounter>>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\LogicalDisk(*)\\% Free Space"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\LogicalDisk(*)\\Free Megabytes", false));

  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\% Disk Read Time"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\% Disk Time"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\% Disk Write Time"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\% Idle Time"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Bytes/Transfer"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Bytes/Read"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Bytes/Write"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Write Queue Length"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Read Queue Length"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Queue Length"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk sec/Transfer"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk sec/Read"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk sec/Write"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Current Disk Queue Length", false));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Disk Transfers/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Disk Reads/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Disk Writes/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Disk Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Disk Read Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Disk Write Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\PhysicalDisk(*)\\Split IO/Sec"));
}

void add_network_related_counters(std::vector<std::unique_ptr<PerformanceDataCounter>>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Bytes Received/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Bytes Sent/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Bytes Total/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Current Bandwidth", false));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Packets/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Packets Received/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Packets Sent/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Packets Received Discarded", false));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Packets Received Errors", false));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Packets Received Unknown", false));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Packets Received Non-Unicast/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Packets Received Unicast/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Packets Sent Unicast/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Network Interface(*)\\Packets Sent Non-Unicast/sec"));
}

void add_memory_related_counters(std::vector<std::unique_ptr<PerformanceDataCounter>>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Memory\\% Committed Bytes In Use"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Memory\\Available MBytes", false));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Memory\\Page Faults/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Memory\\Pages/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Paging File(_Total)\\% Usage"));

  resource_consumption_counters.push_back(std::make_unique<MemoryConsumptionCounter>());
}

void add_process_related_counters(std::vector<std::unique_ptr<PerformanceDataCounter>>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Process(*)\\% Processor Time"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Process(*)\\Elapsed Time"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Process(*)\\ID Process", false));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\Process(*)\\Private Bytes", false));
}

void add_system_related_counters(std::vector<std::unique_ptr<PerformanceDataCounter>>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\% Registry Quota In Use"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\Context Switches/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\File Control Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\File Control Operations/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\File Read Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\File Read Operations/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\File Write Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\File Write Operations/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\File Data Operations/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\Processes", false));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\Processor Queue Length", false));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\System Calls/sec"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\System Up Time"));
  resource_consumption_counters.push_back(PDHCounter::createPDHCounter("\\System\\Threads", false));
}

void PerformanceDataMonitor::addCountersFromPredefinedGroupsProperty(const std::string& predefined_groups) {
  auto groups = utils::string::splitAndTrim(predefined_groups, ",");
  for (const auto& group : groups) {
    if (group == "CPU") {
      add_cpu_related_counters(resource_consumption_counters_);
    } else if (group == "IO") {
      add_io_related_counters(resource_consumption_counters_);
    } else if (group == "Disk") {
      add_disk_related_counters(resource_consumption_counters_);
    } else if (group == "Network") {
      add_network_related_counters(resource_consumption_counters_);
    } else if (group == "Memory") {
      add_memory_related_counters(resource_consumption_counters_);
    } else if (group == "System") {
      add_system_related_counters(resource_consumption_counters_);
    } else if (group == "Process") {
      add_process_related_counters(resource_consumption_counters_);
    } else {
      logger_->log_error("{} is not a valid predefined group for PerformanceDataMonitor", group);
    }
  }
}

void PerformanceDataMonitor::addCustomPDHCountersFromProperty(const std::string& custom_pdh_counters) {
  const auto custom_counters = utils::string::splitAndTrim(custom_pdh_counters, ",");
  for (const auto& custom_counter : custom_counters) {
    auto counter = PDHCounter::createPDHCounter(custom_counter);
    if (counter != nullptr)
      resource_consumption_counters_.push_back(std::move(counter));
  }
}

void PerformanceDataMonitor::setupCountersFromProperties(core::ProcessContext& context) {
  if (const auto custom_pdh_counters = context.getProperty(CustomPDHCounters)) {
    logger_->log_trace("Custom PDH counters configured to be {}", *custom_pdh_counters);
    addCustomPDHCountersFromProperty(*custom_pdh_counters);
  }
}

void PerformanceDataMonitor::setupPredefinedGroupsFromProperties(core::ProcessContext& context) {
  if (const auto predefined_groups = context.getProperty(PredefinedGroups)) {
    logger_->log_trace("Predefined group configured to be {}", *predefined_groups);
    addCountersFromPredefinedGroupsProperty(*predefined_groups);
  }
}

void PerformanceDataMonitor::setupOutputFormatFromProperties(core::ProcessContext& context) {
  if (const auto output_format_string = context.getProperty(OutputFormatProperty)) {
    if (*output_format_string == OPEN_TELEMETRY_FORMAT_STR) {
      output_format_ = OutputFormat::OPENTELEMETRY;
    } else if (*output_format_string == JSON_FORMAT_STR) {
      output_format_ = OutputFormat::JSON;
    } else {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid PerformanceDataMonitor Output Format: " + *output_format_string);
    }
  }

  if (const auto output_compactness_string = context.getProperty(OutputCompactness)) {
    if (*output_compactness_string == PRETTY_FORMAT_STR) {
      pretty_output_ = true;
    } else if (*output_compactness_string == COMPACT_FORMAT_STR) {
      pretty_output_ = false;
    } else {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid PerformanceDataMonitor Output Compactness: " + *output_compactness_string);
    }
  }

  logger_->log_trace("OutputFormat is configured to be {} {}", pretty_output_ ? "pretty" : "compact", output_format_ == OutputFormat::JSON ? "JSON" : "OpenTelemetry");
}

void PerformanceDataMonitor::setupDecimalPlacesFromProperties(core::ProcessContext& context) {
  decimal_places_ = context.getProperty(DecimalPlaces.name) | utils::andThen(parsing::parseIntegral<uint8_t>) | utils::toOptional();

  if (decimal_places_.has_value()) {
    logger_->log_trace("Rounding is enabled with {} decimal places", decimal_places_.value());
  }
}

void PerformanceDataMonitor::setupMembersFromProperties(core::ProcessContext& context) {
  setupCountersFromProperties(context);
  setupPredefinedGroupsFromProperties(context);
  setupOutputFormatFromProperties(context);
  setupDecimalPlacesFromProperties(context);
}

REGISTER_RESOURCE(PerformanceDataMonitor, Processor);

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
