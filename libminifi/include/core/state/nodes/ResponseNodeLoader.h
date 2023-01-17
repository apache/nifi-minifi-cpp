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
#include "core/logging/LoggerConfiguration.h"
#include "core/FlowConfiguration.h"
#include "utils/gsl.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::state::response {

class ResponseNodeLoader {
 public:
  ResponseNodeLoader(std::shared_ptr<Configure> configuration, std::shared_ptr<core::Repository> provenance_repo,
    std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<core::FlowConfiguration> flow_configuration);

  void setNewConfigRoot(core::ProcessGroup* root);
  void clearConfigRoot();
  void setControllerServiceProvider(core::controller::ControllerServiceProvider* controller);
  void setStateMonitor(state::StateMonitor* update_sink);
  std::vector<std::shared_ptr<ResponseNode>> loadResponseNodes(const std::string& clazz) const;
  state::response::NodeReporter::ReportedNode getAgentManifest();

 private:
  void initializeComponentMetrics();
  std::vector<std::shared_ptr<ResponseNode>> getComponentMetricsNodes(const std::string& metrics_class) const;
  std::vector<std::shared_ptr<ResponseNode>> getResponseNodes(const std::string& clazz) const;
  void initializeRepositoryMetrics(const std::shared_ptr<ResponseNode>& response_node) const;
  void initializeQueueMetrics(const std::shared_ptr<ResponseNode>& response_node) const;
  void initializeAgentIdentifier(const std::shared_ptr<ResponseNode>& response_node) const;
  void initializeAgentMonitor(const std::shared_ptr<ResponseNode>& response_node) const;
  void initializeAgentNode(const std::shared_ptr<ResponseNode>& response_node) const;
  void initializeAgentStatus(const std::shared_ptr<ResponseNode>& response_node) const;
  void initializeConfigurationChecksums(const std::shared_ptr<ResponseNode>& response_node) const;
  void initializeFlowMonitor(const std::shared_ptr<ResponseNode>& response_node) const;
  std::vector<std::shared_ptr<ResponseNode>> getMatchingComponentMetricsNodes(const std::string& regex_str) const;

  mutable std::mutex root_mutex_;
  mutable std::mutex component_metrics_mutex_;
  core::ProcessGroup* root_{};
  std::unordered_map<std::string, std::vector<std::shared_ptr<ResponseNode>>> component_metrics_;
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<core::Repository> provenance_repo_;
  std::shared_ptr<core::Repository> flow_file_repo_;
  std::shared_ptr<core::FlowConfiguration> flow_configuration_;
  core::controller::ControllerServiceProvider* controller_{};
  state::StateMonitor* update_sink_{};
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<ResponseNodeLoader>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::state::response
