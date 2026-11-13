// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <vector>
#include <mutex>

#include "FlowStatusRequest.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "core/ProcessGroup.h"
#include "core/BulletinStore.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::c2 {

class FlowStatusBuilder {
 public:
  void setRoot(core::ProcessGroup* root);
  void setBulletinStore(core::BulletinStore* bulletin_store);
  void setRepositoryPaths(const std::filesystem::path& flowfile_repository_path, const std::filesystem::path& content_repository_path);
  void setConfiguration(const std::shared_ptr<Configure>& configuration);
  rapidjson::Document buildFlowStatus(const std::vector<FlowStatusRequest>& requests);

 private:
  void addProcessorStatus(core::Processor* processor, rapidjson::Value& processor_status_list, rapidjson::Document::AllocatorType& allocator,
    const std::unordered_set<FlowStatusQueryOption>& options);
  core::Processor* findProcessor(const std::string& processor_id);
  std::expected<void, std::string> addProcessorStatuses(rapidjson::Value& processor_status_list, rapidjson::Document::AllocatorType& allocator,
    const std::string& identifier, const std::unordered_set<FlowStatusQueryOption>& options);
  static void addConnectionStatus(Connection* connection, rapidjson::Value& connection_status_list, rapidjson::Document::AllocatorType& allocator,
    const std::unordered_set<FlowStatusQueryOption>& options);
  std::expected<void, std::string> addConnectionStatuses(rapidjson::Value& connection_status_list, rapidjson::Document::AllocatorType& allocator,
    const std::string& identifier, const std::unordered_set<FlowStatusQueryOption>& options);
  std::expected<void, std::string> addInstanceStatus(rapidjson::Value& instance_status, rapidjson::Document::AllocatorType& allocator, const std::unordered_set<FlowStatusQueryOption>& options);
  std::expected<void, std::string> addSystemDiagnosticsStatus(rapidjson::Value& system_diagnostics_status, rapidjson::Document::AllocatorType& allocator,
    const std::unordered_set<FlowStatusQueryOption>& options);

  std::mutex root_mutex_;
  core::BulletinStore* bulletin_store_{};
  std::filesystem::path flowfile_repository_path_;
  std::filesystem::path content_repository_path_;
  std::map<std::string, Connection*> connection_map_;
  std::unordered_set<Connection*> connections_;
  std::vector<core::Processor*> processors_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FlowStatusBuilder>::getLogger();
};

}  // namespace org::apache::nifi::minifi::c2
