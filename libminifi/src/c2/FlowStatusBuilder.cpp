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

#include "c2/FlowStatusBuilder.h"

#include <filesystem>

#include "utils/expected.h"
#include "utils/Id.h"
#include "utils/OsUtils.h"

namespace org::apache::nifi::minifi::c2 {

void FlowStatusBuilder::setRoot(core::ProcessGroup* root) {
  std::lock_guard<std::mutex> guard(root_mutex_);
  processors_.clear();
  connection_map_.clear();
  connections_.clear();
  if (!root) {
    return;
  }
  root->getConnections(connection_map_);
  std::transform(connection_map_.begin(), connection_map_.end(), std::inserter(connections_, connections_.begin()), [](const auto& pair) { return pair.second; });
  root->getAllProcessors(processors_);
}

void FlowStatusBuilder::setBulletinStore(core::BulletinStore* bulletin_store) {
  bulletin_store_ = bulletin_store;
}

void FlowStatusBuilder::setRepositoryPaths(const std::filesystem::path& flowfile_repository_path, const std::filesystem::path& content_repository_path) {
  flowfile_repository_path_ = flowfile_repository_path;
  content_repository_path_ = content_repository_path;
}

void FlowStatusBuilder::addProcessorStatus(core::Processor* processor, rapidjson::Value& processor_status_list, rapidjson::Document::AllocatorType& allocator,
    const std::unordered_set<FlowStatusQueryOption>& options) {
  if (!processor) {
    return;
  }
  rapidjson::Value processor_status(rapidjson::kObjectType);
  processor_status.AddMember("id", rapidjson::Value(processor->getUUIDStr().c_str(), allocator), allocator);
  processor_status.AddMember("name", rapidjson::Value(processor->getName().c_str(), allocator), allocator);

  std::vector<core::Bulletin> bulletins;
  if (bulletin_store_) {
    bulletins = bulletin_store_->getBulletinsForProcessor(processor->getUUIDStr());
  }

  if (options.contains(FlowStatusQueryOption::health)) {
    processor_status.AddMember("processorHealth", rapidjson::Value(rapidjson::kObjectType), allocator);
    processor_status["processorHealth"].AddMember("runStatus", processor->isRunning() ? rapidjson::Value("Running") : rapidjson::Value("Stopped"), allocator);
    processor_status["processorHealth"].AddMember("hasBulletins", bulletins.empty() ? rapidjson::Value(false) : rapidjson::Value(true), allocator);
  } else {
    processor_status.AddMember("processorHealth", rapidjson::Value(rapidjson::kNullType), allocator);
  }

  if (options.contains(FlowStatusQueryOption::stats)) {
    processor_status.AddMember("processorStats", rapidjson::Value(rapidjson::kObjectType), allocator);
    auto metrics = processor->getMetrics();
    processor_status["processorStats"].AddMember<uint64_t>("flowfilesReceived", metrics->incomingFlowFiles().load(), allocator);
    processor_status["processorStats"].AddMember<uint64_t>("flowfilesSent", metrics->transferredFlowFiles().load(), allocator);
    processor_status["processorStats"].AddMember<uint64_t>("bytesRead", metrics->bytesRead().load(), allocator);
    processor_status["processorStats"].AddMember<uint64_t>("bytesWritten", metrics->bytesWritten().load(), allocator);
    processor_status["processorStats"].AddMember<uint64_t>("bytesReceived", metrics->incomingBytes().load(), allocator);
    processor_status["processorStats"].AddMember<uint64_t>("bytesTransferred", metrics->transferredBytes().load(), allocator);
    processor_status["processorStats"].AddMember<uint64_t>("invocations", metrics->invocations().load(), allocator);
    processor_status["processorStats"].AddMember<uint64_t>("processingNanos", metrics->processingNanos().load(), allocator);
  } else {
    processor_status.AddMember("processorStats", rapidjson::Value(rapidjson::kNullType), allocator);
  }

  if (options.contains(FlowStatusQueryOption::bulletins)) {
    processor_status.AddMember("bulletinList", rapidjson::Value(rapidjson::kArrayType), allocator);
    for (const auto& bulletin : bulletins) {
      rapidjson::Value bulletin_node(rapidjson::kObjectType);
      bulletin_node.AddMember<int64_t>("timestamp", std::chrono::duration_cast<std::chrono::seconds>(bulletin.timestamp.time_since_epoch()).count(), allocator);
      bulletin_node.AddMember("message", bulletin.message, allocator);
      processor_status["bulletinList"].PushBack(bulletin_node, allocator);
    }
  } else {
    processor_status.AddMember("bulletinList", rapidjson::Value(rapidjson::kNullType), allocator);
  }
  processor_status_list.PushBack(processor_status, allocator);
}

core::Processor* FlowStatusBuilder::findProcessor(const std::string& processor_id) {
  auto id_opt = minifi::utils::Identifier::parse(processor_id);
  auto it = std::find_if(processors_.begin(), processors_.end(), [&](const auto processor) {
    return processor->getName() == processor_id || (id_opt && processor->getUUID() == id_opt.value());
  });
  if (it != processors_.end()) {
    return *it;
  }
  return nullptr;
}

nonstd::expected<void, std::string> FlowStatusBuilder::addProcessorStatuses(rapidjson::Value& processor_status_list, rapidjson::Document::AllocatorType& allocator,
    const std::string& identifier, const std::unordered_set<FlowStatusQueryOption>& options) {
  static const std::unordered_set<FlowStatusQueryOption> valid_options = {
    FlowStatusQueryOption::health,
    FlowStatusQueryOption::stats,
    FlowStatusQueryOption::bulletins
  };

  for (const auto& option : options) {
    if (!valid_options.contains(option)) {
      logger_->log_error("Unable to get processorStatus: Invalid query option for processor status '{}'", magic_enum::enum_name(option));
      return nonstd::make_unexpected(fmt::format("Unable to get processorStatus: Invalid query option for processor status '{}'", magic_enum::enum_name(option)));
    }
  }

  std::lock_guard<std::mutex> guard(root_mutex_);
  if (identifier.empty()) {
    logger_->log_error("Unable to get processorStatus: Query is incomplete");
    return nonstd::make_unexpected("Unable to get processorStatus: Query is incomplete");
  } else if (identifier == "all") {
    for (const auto processor : processors_) {
      addProcessorStatus(processor, processor_status_list, allocator, options);
    }
  } else {
    auto processor = findProcessor(identifier);
    if (!processor) {
      logger_->log_error("Unable to get processorStatus: No processor with key '{}' to report status on", identifier);
      return nonstd::make_unexpected(fmt::format("Unable to get processorStatus: No processor with key '{}' to report status on", identifier));
    }
    addProcessorStatus(processor, processor_status_list, allocator, options);
  }

  return {};
}

void FlowStatusBuilder::addConnectionStatus(Connection* connection, rapidjson::Value& connection_status_list, rapidjson::Document::AllocatorType& allocator,
    const std::unordered_set<FlowStatusQueryOption>& options) {
  if (!connection) {
    return;
  }
  rapidjson::Value connection_status(rapidjson::kObjectType);
  connection_status.AddMember("id", rapidjson::Value(connection->getUUIDStr().c_str(), allocator), allocator);
  connection_status.AddMember("name", rapidjson::Value(connection->getName().c_str(), allocator), allocator);

  if (options.contains(FlowStatusQueryOption::health)) {
    connection_status.AddMember("connectionHealth", rapidjson::Value(rapidjson::kObjectType), allocator);
    connection_status["connectionHealth"].AddMember("queuedCount", connection->getQueueSize(), allocator);
    connection_status["connectionHealth"].AddMember("queuedBytes", connection->getQueueDataSize(), allocator);
  } else {
    connection_status.AddMember("connectionHealth", rapidjson::Value(rapidjson::kNullType), allocator);
  }

  connection_status_list.PushBack(connection_status, allocator);
}

nonstd::expected<void, std::string> FlowStatusBuilder::addConnectionStatuses(rapidjson::Value& connection_status_list, rapidjson::Document::AllocatorType& allocator,
    const std::string& identifier, const std::unordered_set<FlowStatusQueryOption>& options) {
  for (const auto& option : options) {
    if (option != FlowStatusQueryOption::health) {
      logger_->log_error("Unable to get connectionStatus: Invalid query option for connection status '{}'", magic_enum::enum_name(option));
      return nonstd::make_unexpected(fmt::format("Unable to get connectionStatus: Invalid query option for connection status '{}'", magic_enum::enum_name(option)));
    }
  }
  std::lock_guard<std::mutex> guard(root_mutex_);
  if (identifier.empty()) {
    logger_->log_error("Unable to get connectionStatus: Query is incomplete");
    return nonstd::make_unexpected("Unable to get connectionStatus: Query is incomplete");
  }

  if (identifier == "all") {
    for (const auto connection : connections_) {
      addConnectionStatus(connection, connection_status_list, allocator, options);
    }
  } else {
    if (connection_map_.contains(identifier)) {
      addConnectionStatus(connection_map_[identifier], connection_status_list, allocator, options);
    } else {
      logger_->log_error("Unable to get connectionStatus: No connection with key '{}' to report status on", identifier);
      return nonstd::make_unexpected(fmt::format("Unable to get connectionStatus: No connection with key '{}' to report status on", identifier));
    }
  }

  return {};
}

nonstd::expected<void, std::string> FlowStatusBuilder::addInstanceStatus(rapidjson::Value& instance_status, rapidjson::Document::AllocatorType& allocator,
   const std::unordered_set<FlowStatusQueryOption>& options) {
  static const std::unordered_set<FlowStatusQueryOption> valid_options = {
    FlowStatusQueryOption::health,
    FlowStatusQueryOption::stats,
    FlowStatusQueryOption::bulletins
  };

  for (const auto& option : options) {
    if (!valid_options.contains(option)) {
      logger_->log_error("Unable to get instance: Invalid query option for instance status '{}'", magic_enum::enum_name(option));
      return nonstd::make_unexpected(fmt::format("Unable to get instance: Invalid query option for instance status '{}'", magic_enum::enum_name(option)));
    }
  }

  if (options.empty()) {
    return {};
  }

  instance_status = rapidjson::Value(rapidjson::kObjectType);
  std::deque<core::Bulletin> bulletins;
  if (bulletin_store_) {
    bulletins = bulletin_store_->getBulletins(std::chrono::minutes(5));
  }

  if (options.contains(FlowStatusQueryOption::health)) {
    instance_status.AddMember("instanceHealth", rapidjson::Value(rapidjson::kObjectType), allocator);
    uint64_t queued_count = 0;
    uint64_t queued_bytes = 0;
    for (const auto connection : connections_) {
      queued_count += connection->getQueueSize();
      queued_bytes += connection->getQueueDataSize();
    }

    instance_status["instanceHealth"].AddMember("queuedCount", queued_count, allocator);
    instance_status["instanceHealth"].AddMember("queuedContentSize", queued_bytes, allocator);
    instance_status["instanceHealth"].AddMember("hasBulletins", bulletins.empty() ? rapidjson::Value(false) : rapidjson::Value(true), allocator);
  } else {
    instance_status.AddMember("instanceHealth", rapidjson::Value(rapidjson::kNullType), allocator);
  }

  if (options.contains(FlowStatusQueryOption::bulletins)) {
    instance_status.AddMember("bulletinList", rapidjson::Value(rapidjson::kArrayType), allocator);
    for (const auto& bulletin : bulletins) {
      rapidjson::Value bulletin_node(rapidjson::kObjectType);
      bulletin_node.AddMember<int64_t>("timestamp", std::chrono::duration_cast<std::chrono::seconds>(bulletin.timestamp.time_since_epoch()).count(), allocator);
      bulletin_node.AddMember("message", bulletin.message, allocator);
      instance_status["bulletinList"].PushBack(bulletin_node, allocator);
    }
  } else {
    instance_status.AddMember("bulletinList", rapidjson::Value(rapidjson::kNullType), allocator);
  }

  if (options.contains(FlowStatusQueryOption::stats)) {
    instance_status.AddMember("instanceStats", rapidjson::Value(rapidjson::kObjectType), allocator);

    uint64_t flowfiles_transferred = 0;
    uint64_t bytes_read = 0;
    uint64_t bytes_written = 0;
    uint64_t transferred_bytes = 0;
    for (auto processor : processors_) {
      auto metrics = processor->getMetrics();
      flowfiles_transferred += metrics->transferredFlowFiles().load();
      bytes_read += metrics->bytesRead().load();
      bytes_written += metrics->bytesWritten().load();
      transferred_bytes += metrics->transferredBytes().load();
    }

    instance_status["instanceStats"].AddMember<uint64_t>("flowfilesTransferred", flowfiles_transferred, allocator);
    instance_status["instanceStats"].AddMember<uint64_t>("bytesRead", bytes_read, allocator);
    instance_status["instanceStats"].AddMember<uint64_t>("bytesWritten", bytes_written, allocator);
    instance_status["instanceStats"].AddMember<uint64_t>("bytesTransferred", transferred_bytes, allocator);
  } else {
    instance_status.AddMember("instanceStats", rapidjson::Value(rapidjson::kNullType), allocator);
  }

  return {};
}

nonstd::expected<void, std::string> FlowStatusBuilder::addSystemDiagnosticsStatus(rapidjson::Value& system_diagnostics_status, rapidjson::Document::AllocatorType& allocator,
    const std::unordered_set<FlowStatusQueryOption>& options) {
  static const std::unordered_set<FlowStatusQueryOption> valid_options = {
    FlowStatusQueryOption::processorstats,
    FlowStatusQueryOption::flowfilerepositoryusage,
    FlowStatusQueryOption::contentrepositoryusage
  };

  for (const auto& option : options) {
    if (!valid_options.contains(option)) {
      logger_->log_error("Unable to get systemDiagnostics: Invalid query option for system diagnostics '{}'", magic_enum::enum_name(option));
      return nonstd::make_unexpected(fmt::format("Unable to get systemDiagnostics: Invalid query option for system diagnostics '{}'", magic_enum::enum_name(option)));
    }
  }

  if (options.empty()) {
    return {};
  }

  system_diagnostics_status = rapidjson::Value(rapidjson::kObjectType);
  if (options.contains(FlowStatusQueryOption::processorstats)) {
    system_diagnostics_status.AddMember("processorStatus", rapidjson::Value(rapidjson::kObjectType), allocator);
    auto load_average = utils::OsUtils::getSystemLoadAverage();
    system_diagnostics_status["processorStatus"].AddMember("loadAverage", load_average ? load_average.value() : -1.0, allocator);
    uint32_t cores = (std::max)(uint32_t{1}, std::thread::hardware_concurrency());
    system_diagnostics_status["processorStatus"].AddMember("availableProcessors", cores, allocator);
  } else {
    system_diagnostics_status.AddMember("processorStatus", rapidjson::Value(rapidjson::kNullType), allocator);
  }

  if (options.contains(FlowStatusQueryOption::flowfilerepositoryusage)) {
    system_diagnostics_status.AddMember("flowfileRepositoryUsage", rapidjson::Value(rapidjson::kObjectType), allocator);
    if (!flowfile_repository_path_.empty()) {
      auto usage = std::filesystem::space(flowfile_repository_path_);
      system_diagnostics_status["flowfileRepositoryUsage"].AddMember("freeSpace", static_cast<uint64_t>(usage.free), allocator);
      system_diagnostics_status["flowfileRepositoryUsage"].AddMember("totalSpace", static_cast<uint64_t>(usage.capacity), allocator);
      system_diagnostics_status["flowfileRepositoryUsage"].AddMember("usedSpace", static_cast<uint64_t>(usage.capacity - usage.free), allocator);
      system_diagnostics_status["flowfileRepositoryUsage"].AddMember("diskUtilization",
        static_cast<uint64_t>(static_cast<double>(usage.capacity - usage.free) / static_cast<double>(usage.capacity) * 100), allocator);
    } else {
      system_diagnostics_status["flowfileRepositoryUsage"].AddMember("freeSpace", -1, allocator);
      system_diagnostics_status["flowfileRepositoryUsage"].AddMember("totalSpace", -1, allocator);
      system_diagnostics_status["flowfileRepositoryUsage"].AddMember("usedSpace", -1, allocator);
      system_diagnostics_status["flowfileRepositoryUsage"].AddMember("diskUtilization", -1, allocator);
    }
  } else {
    system_diagnostics_status.AddMember("flowfileRepositoryUsage", rapidjson::Value(rapidjson::kNullType), allocator);
  }

  if (options.contains(FlowStatusQueryOption::contentrepositoryusage)) {
    system_diagnostics_status.AddMember("contentRepositoryUsage", rapidjson::Value(rapidjson::kObjectType), allocator);
    if (!content_repository_path_.empty()) {
      auto usage = std::filesystem::space(content_repository_path_);
      system_diagnostics_status["contentRepositoryUsage"].AddMember("freeSpace", static_cast<uint64_t>(usage.free), allocator);
      system_diagnostics_status["contentRepositoryUsage"].AddMember("totalSpace", static_cast<uint64_t>(usage.capacity), allocator);
      system_diagnostics_status["contentRepositoryUsage"].AddMember("usedSpace", static_cast<uint64_t>(usage.capacity - usage.free), allocator);
      system_diagnostics_status["contentRepositoryUsage"].AddMember("diskUtilization",
        static_cast<uint64_t>(static_cast<double>(usage.capacity - usage.free) / static_cast<double>(usage.capacity) * 100), allocator);
    } else {
      system_diagnostics_status["contentRepositoryUsage"].AddMember("freeSpace", -1, allocator);
      system_diagnostics_status["contentRepositoryUsage"].AddMember("totalSpace", -1, allocator);
      system_diagnostics_status["contentRepositoryUsage"].AddMember("usedSpace", -1, allocator);
      system_diagnostics_status["contentRepositoryUsage"].AddMember("diskUtilization", -1, allocator);
    }
  } else {
    system_diagnostics_status.AddMember("contentRepositoryUsage", rapidjson::Value(rapidjson::kNullType), allocator);
  }

  return {};
}

rapidjson::Document FlowStatusBuilder::buildFlowStatus(const std::vector<FlowStatusRequest>& requests) {
  rapidjson::Document doc;
  doc.SetObject();

  auto allocator = doc.GetAllocator();

  auto handleError = [&doc, &allocator](const nonstd::expected<void, std::string>& result) {
    if (result) {
      return;
    }
    doc["errorsGeneratingReport"].GetArray().PushBack(rapidjson::Value(result.error().c_str(), allocator), allocator);
  };

  doc.AddMember("controllerServiceStatusList", rapidjson::Value(rapidjson::kNullType), allocator);
  doc.AddMember("connectionStatusList", rapidjson::Value(rapidjson::kNullType), allocator);
  doc.AddMember("remoteProcessGroupStatusList", rapidjson::Value(rapidjson::kNullType), allocator);
  doc.AddMember("instanceStatus", rapidjson::Value(rapidjson::kNullType), allocator);
  doc.AddMember("systemDiagnosticsStatus", rapidjson::Value(rapidjson::kNullType), allocator);
  doc.AddMember("processorStatusList", rapidjson::Value(rapidjson::kNullType), allocator);
  doc.AddMember("errorsGeneratingReport", rapidjson::Value(rapidjson::kArrayType), allocator);

  for (const auto& request : requests) {
    if (request.query_type == FlowStatusQueryType::processor) {
      doc["processorStatusList"] = rapidjson::Value(rapidjson::kArrayType);
      handleError(addProcessorStatuses(doc["processorStatusList"], allocator, request.identifier, request.options));
    } else if (request.query_type == FlowStatusQueryType::connection) {
      doc["connectionStatusList"] = rapidjson::Value(rapidjson::kArrayType);
      handleError(addConnectionStatuses(doc["connectionStatusList"], allocator, request.identifier, request.options));
    } else if (request.query_type == FlowStatusQueryType::instance) {
      handleError(addInstanceStatus(doc["instanceStatus"], allocator, request.options));
    } else if (request.query_type == FlowStatusQueryType::systemdiagnostics) {
      handleError(addSystemDiagnosticsStatus(doc["systemDiagnosticsStatus"], allocator, request.options));
    }
  }

  return doc;
}

}  // namespace org::apache::nifi::minifi::c2
