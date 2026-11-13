// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <unordered_map>
#include <map>
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <vector>

#include "MetricsBase.h"
#include "core/ProcessGroup.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "core/FlowConfiguration.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/Id.h"
#include "utils/expected.h"
#include "minifi-cpp/core/RepositoryMetricsSource.h"
#include "utils/file/AssetManager.h"
#include "minifi-cpp/core/state/nodes/ResponseNodeLoader.h"
#include "core/BulletinStore.h"

namespace org::apache::nifi::minifi::state::response {

class ResponseNodeLoaderImpl : public ResponseNodeLoader {
 public:
  ResponseNodeLoaderImpl(std::shared_ptr<Configure> configuration, std::vector<std::shared_ptr<core::RepositoryMetricsSource>> repository_metric_sources,
    std::shared_ptr<core::FlowConfiguration> flow_configuration, utils::file::AssetManager* asset_manager = nullptr, core::BulletinStore* bulletin_store = nullptr);

  void setNewConfigRoot(core::ProcessGroup* root) override;
  void clearConfigRoot() override;
  void setControllerServiceProvider(core::controller::ControllerServiceProvider* controller) override;
  void setStateMonitor(state::StateMonitor* update_sink) override;
  std::vector<SharedResponseNode> loadResponseNodes(const std::string& clazz, core::ProcessGroup* root) override;
  state::response::NodeReporter::ReportedNode getAgentManifest() const override;

 private:
  void initializeComponentMetrics();
  std::vector<SharedResponseNode> getComponentMetricsNodes(const std::string& metrics_class) const;
  std::expected<SharedResponseNode, std::string> getSystemMetricsNode(const std::string& clazz);
  std::vector<SharedResponseNode> getResponseNodes(const std::string& clazz);
  void initializeRepositoryMetrics(const SharedResponseNode& response_node) const;
  void initializeQueueMetrics(const SharedResponseNode& response_node) const;
  void initializeAgentIdentifier(const SharedResponseNode& response_node) const;
  void initializeAgentMonitor(const SharedResponseNode& response_node) const;
  void initializeAgentNode(const SharedResponseNode& response_node) const;
  void initializeAgentStatus(const SharedResponseNode& response_node) const;
  void initializeConfigurationChecksums(const SharedResponseNode& response_node) const;
  void initializeFlowInformation(const SharedResponseNode& response_node) const;
  void initializeAssetInformation(const SharedResponseNode& response_node) const;
  std::vector<SharedResponseNode> getMatchingComponentMetricsNodes(const std::string& regex_str) const;

  mutable std::mutex root_mutex_;
  mutable std::mutex component_metrics_mutex_;
  mutable std::mutex system_metrics_mutex_;
  mutable std::mutex initialization_mutex_;
  core::ProcessGroup* root_{};
  std::unordered_map<std::string, std::vector<SharedResponseNode>> component_metrics_;
  std::unordered_map<std::string, SharedResponseNode> system_metrics_;
  std::unordered_set<std::string> initialized_metrics_;
  std::shared_ptr<Configure> configuration_;
  std::vector<std::shared_ptr<core::RepositoryMetricsSource>> repository_metric_sources_;
  std::shared_ptr<core::FlowConfiguration> flow_configuration_;
  utils::file::AssetManager* asset_manager_{};
  core::controller::ControllerServiceProvider* controller_{};
  state::StateMonitor* update_sink_{};
  core::BulletinStore* bulletin_store_{};
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<ResponseNodeLoader>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::state::response
