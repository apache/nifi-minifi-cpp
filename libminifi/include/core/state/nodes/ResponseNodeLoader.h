/**
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
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "core/FlowConfiguration.h"
#include "utils/gsl.h"
#include "utils/Id.h"
#include "utils/expected.h"
#include "core/RepositoryMetricsSource.h"
#include "utils/file/AssetManager.h"
#include "minifi-cpp/core/state/nodes/ResponseNodeLoader.h"

namespace org::apache::nifi::minifi::state::response {

class ResponseNodeLoaderImpl : public ResponseNodeLoader {
 public:
  ResponseNodeLoaderImpl(std::shared_ptr<Configure> configuration, std::vector<std::shared_ptr<core::RepositoryMetricsSource>> repository_metric_sources,
    std::shared_ptr<core::FlowConfiguration> flow_configuration, utils::file::AssetManager* asset_manager = nullptr);

  void setNewConfigRoot(core::ProcessGroup* root) override;
  void clearConfigRoot() override;
  void setControllerServiceProvider(core::controller::ControllerServiceProvider* controller) override;
  void setStateMonitor(state::StateMonitor* update_sink) override;
  std::vector<SharedResponseNode> loadResponseNodes(const std::string& clazz) override;
  state::response::NodeReporter::ReportedNode getAgentManifest() const override;

 private:
  void initializeComponentMetrics();
  std::vector<SharedResponseNode> getComponentMetricsNodes(const std::string& metrics_class) const;
  nonstd::expected<SharedResponseNode, std::string> getSystemMetricsNode(const std::string& clazz);
  std::vector<SharedResponseNode> getResponseNodes(const std::string& clazz);
  void initializeRepositoryMetrics(const SharedResponseNode& response_node) const;
  void initializeQueueMetrics(const SharedResponseNode& response_node) const;
  void initializeAgentIdentifier(const SharedResponseNode& response_node) const;
  void initializeAgentMonitor(const SharedResponseNode& response_node) const;
  void initializeAgentNode(const SharedResponseNode& response_node) const;
  void initializeAgentStatus(const SharedResponseNode& response_node) const;
  void initializeConfigurationChecksums(const SharedResponseNode& response_node) const;
  void initializeFlowMonitor(const SharedResponseNode& response_node) const;
  void initializeAssetInformation(const SharedResponseNode& response_node) const;
  std::vector<SharedResponseNode> getMatchingComponentMetricsNodes(const std::string& regex_str) const;

  mutable std::mutex root_mutex_;
  mutable std::mutex component_metrics_mutex_;
  mutable std::mutex system_metrics_mutex_;
  core::ProcessGroup* root_{};
  std::unordered_map<std::string, std::vector<SharedResponseNode>> component_metrics_;
  std::unordered_map<std::string, SharedResponseNode> system_metrics_;
  std::shared_ptr<Configure> configuration_;
  std::vector<std::shared_ptr<core::RepositoryMetricsSource>> repository_metric_sources_;
  std::shared_ptr<core::FlowConfiguration> flow_configuration_;
  utils::file::AssetManager* asset_manager_{};
  core::controller::ControllerServiceProvider* controller_{};
  state::StateMonitor* update_sink_{};
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<ResponseNodeLoader>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::state::response
