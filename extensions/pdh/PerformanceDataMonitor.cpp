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

#include "PerformanceDataMonitor.h"
#include "PDHCounters.h"
#include "MemoryConsumptionCounter.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Relationship PerformanceDataMonitor::Success("success", "All files are routed to success");

core::Property PerformanceDataMonitor::PredefinedGroups(
    core::PropertyBuilder::createProperty("PredefinedGroups")->
    withDescription("Comma separated list to which predefined groups should be included")->
    withDefaultValue("")->build());

core::Property PerformanceDataMonitor::CustomPDHCounters(
    core::PropertyBuilder::createProperty("CustomPDHCounters")->
    withDescription("Comma separated list of PDHCounters to collect from")->
    withDefaultValue("")->build());

core::Property PerformanceDataMonitor::OutputFormatProperty(
    core::PropertyBuilder::createProperty("OutputFormat")->
    withDescription("Format of the created flowfiles")->
    withAllowableValue<std::string>(JSON_FORMAT_STR)->
    withAllowableValue(OPEN_TELEMETRY_FORMAT_STR)->
    withDefaultValue(JSON_FORMAT_STR)->build());

PerformanceDataMonitor::~PerformanceDataMonitor() {
  if (pdh_query_ != nullptr)
    PdhCloseQuery(pdh_query_);
  for (PerformanceDataCounter* resource_consumption_counter : resource_consumption_counters_) {
    delete resource_consumption_counter;
  }
}

void PerformanceDataMonitor::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) {
  setupMembersFromProperties(context);

  std::vector<PerformanceDataCounter*> valid_counters;

  for (PerformanceDataCounter* counter : resource_consumption_counters_) {
    PDHCounterBase* pdh_counter = dynamic_cast<PDHCounterBase*> (counter);
    if (pdh_counter != nullptr) {
      if (pdh_query_ == nullptr)
        PdhOpenQuery(NULL, NULL, &pdh_query_);
      PDH_STATUS add_to_query_result = pdh_counter->addToQuery(pdh_query_);
      if (add_to_query_result != ERROR_SUCCESS) {
        logger_->log_error(("Error adding " + pdh_counter->getName() + " to query: " + std::to_string(add_to_query_result)).c_str());
        delete counter;
      } else {
        valid_counters.push_back(counter);
      }
    } else {
      valid_counters.push_back(counter);
    }
  }
  resource_consumption_counters_ = valid_counters;
  PdhCollectQueryData(pdh_query_);
}

void PerformanceDataMonitor::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  if (resource_consumption_counters_.empty()) {
    logger_->log_error("No valid counters for PerformanceDataMonitor");
    return;
  }

  std::shared_ptr<core::FlowFile> flowFile = session->create();
  if (!flowFile) {
    logger_->log_error("Failed to create flowfile!");
    return;
  }

  if (pdh_query_ != nullptr)
    PdhCollectQueryData(pdh_query_);

  rapidjson::Document root = rapidjson::Document(rapidjson::kObjectType);
  rapidjson::Value& body = prepareJSONBody(root);
  for (PerformanceDataCounter* counter : resource_consumption_counters_) {
    PDHCounterBase* pdh_counter = dynamic_cast<PDHCounterBase*>(counter);
    if (pdh_counter != nullptr && pdh_counter->collectData() != ERROR_SUCCESS)
      continue;
    counter->addToJson(body, root.GetAllocator());
  }
  PerformanceDataMonitor::WriteCallback callback(std::move(root));
  session->write(flowFile, &callback);
  session->transfer(flowFile, Success);
}

void PerformanceDataMonitor::initialize(void) {
  setSupportedProperties({ CustomPDHCounters, PredefinedGroups, OutputFormatProperty });
  setSupportedRelationships({ PerformanceDataMonitor::Success });
}

rapidjson::Value& PerformanceDataMonitor::prepareJSONBody(rapidjson::Document& root) {
  switch (output_format_) {
    case (OutputFormat::kOpenTelemetry):
      root.AddMember("Name", "PerformanceData", root.GetAllocator());
      root.AddMember("Timestamp", std::time(0), root.GetAllocator());
      root.AddMember("Body", rapidjson::Value{ rapidjson::kObjectType }, root.GetAllocator());
      return root["Body"];
    case (OutputFormat::kJSON):
      return root;
    default:
      return root;
  }
}

void add_cpu_related_counters(std::vector<PerformanceDataCounter*>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Processor(*)\\% Processor Time"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Processor(*)\\% User Time"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Processor(*)\\% Privileged Time"));
}

void add_io_related_counters(std::vector<PerformanceDataCounter*>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Process(_Total)\\IO Read Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Process(_Total)\\IO Write Bytes/sec"));
}

void add_disk_related_counters(std::vector<PerformanceDataCounter*>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\LogicalDisk(*)\\% Free Space"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\LogicalDisk(*)\\Free Megabytes", false));

  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\% Disk Read Time"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\% Disk Time"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\% Disk Write Time"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\% Idle Time"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Bytes/Transfer"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Bytes/Read"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Bytes/Write"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Write Queue Length"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Read Queue Length"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk Queue Length"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk sec/Transfer"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk sec/Read"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Avg. Disk sec/Write"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Current Disk Queue Length"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Disk Transfers/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Disk Reads/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Disk Writes/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Disk Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Disk Read Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Disk Write Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\PhysicalDisk(*)\\Split IO/Sec"));
}

void add_network_related_counters(std::vector<PerformanceDataCounter*>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Bytes Received/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Bytes Sent/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Bytes Total/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Current Bandwidth"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Packets/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Packets Received/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Packets Sent/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Packets Received Discarded", false));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Packets Received Errors", false));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Packets Received Unknown", false));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Packets Received Non-Unicast/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Packets Received Unicast/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Packets Sent Unicast/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Network Interface(*)\\Packets Sent Non-Unicast/sec"));
}

void add_memory_related_counters(std::vector<PerformanceDataCounter*>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Memory\\% Committed Bytes In Use"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Memory\\Available MBytes"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Memory\\Page Faults/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Memory\\Pages/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Paging File(_Total)\\% Usage"));

  resource_consumption_counters.push_back(new MemoryConsumptionCounter());
}

void add_process_related_counters(std::vector<PerformanceDataCounter*>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Process(*)\\% Processor Time"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Process(*)\\Elapsed Time"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Process(*)\\ID Process", false));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\Process(*)\\Private Bytes", false));
}

void add_system_related_counters(std::vector<PerformanceDataCounter*>& resource_consumption_counters) {
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\% Registry Quota In Use"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\Context Switches/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\File Control Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\File Control Operations/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\File Read Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\File Read Operations/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\File Write Bytes/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\File Write Operations/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\File Data Operations/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\Processes", false));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\Processor Queue Length", false));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\System Calls/sec"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\System Up Time"));
  resource_consumption_counters.push_back(PDHCounterBase::createPDHCounter("\\System\\Threads", false));
}

void PerformanceDataMonitor::addCountersFromPredefinedGroupsProperty(const std::string& predefined_groups) {
  auto groups = utils::StringUtils::split(predefined_groups, ",");
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
      logger_->log_error((group + " is not a valid predefined group for PerformanceDataMonitor").c_str());
    }
  }
}

void PerformanceDataMonitor::addCustomPDHCountersFromProperty(const std::string& custom_pdh_counters) {
  const auto custom_counters = utils::StringUtils::split(custom_pdh_counters, ",");
  for (const auto& custom_counter : custom_counters) {
    PDHCounterBase* counter = PDHCounterBase::createPDHCounter(custom_counter);
    if (counter != nullptr)
      resource_consumption_counters_.push_back(counter);
  }
}

void PerformanceDataMonitor::setupMembersFromProperties(const std::shared_ptr<core::ProcessContext>& context) {
  std::string predefined_groups;
  if (context->getProperty(PredefinedGroups.getName(), predefined_groups)) {
    logger_->log_trace("Predefined group configured to be %s", predefined_groups);
    addCountersFromPredefinedGroupsProperty(predefined_groups);
  }

  std::string custom_pdh_counters;
  if (context->getProperty(CustomPDHCounters.getName(), custom_pdh_counters)) {
    logger_->log_trace("Custom PDH counters configured to be %s", custom_pdh_counters);
    addCustomPDHCountersFromProperty(custom_pdh_counters);
  }

  std::string output_format_string;
  if (context->getProperty(OutputFormatProperty.getName(), output_format_string)) {
    if (output_format_string == OPEN_TELEMETRY_FORMAT_STR) {
      logger_->log_trace("OutputFormat is configured to be OpenTelemetry");
      output_format_ = OutputFormat::kOpenTelemetry;
    } else if (output_format_string == JSON_FORMAT_STR) {
      logger_->log_trace("OutputFormat is configured to be JSON");
      output_format_ = OutputFormat::kJSON;
    } else {
      logger_->log_error("Invalid OutputFormat, defaulting to JSON");
      output_format_ = OutputFormat::kJSON;
    }
  }
}

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
