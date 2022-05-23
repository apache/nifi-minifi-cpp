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
    std::shared_ptr<core::Repository> flow_file_repo, core::FlowConfiguration* flow_configuration);
  std::shared_ptr<ResponseNode> loadResponseNode(const std::string& clazz, core::ProcessGroup* root);
  std::shared_ptr<state::response::ResponseNode> getComponentMetricsNode(const std::string& metrics_class) const;
  void setControllerServiceProvider(core::controller::ControllerServiceProvider* controller);
  void setStateMonitor(state::StateMonitor* update_sink);
  utils::Identifier registerFlowChangeCallback(const std::function<void(core::ProcessGroup*)> cb);
  void unregisterFlowChangeCallback(const utils::Identifier& uuid);
  void flowChanged(core::ProcessGroup* root);

 private:
  void initializeComponentMetrics(core::ProcessGroup* root);
  std::shared_ptr<ResponseNode> getResponseNode(const std::string& clazz);
  void initializeRepositoryMetrics(const std::shared_ptr<ResponseNode>& response_node);
  void initializeQueueMetrics(const std::shared_ptr<ResponseNode>& response_node, core::ProcessGroup* root);
  void initializeAgentIdentifier(const std::shared_ptr<ResponseNode>& response_node);
  void initializeAgentMonitor(const std::shared_ptr<ResponseNode>& response_node);
  void initializeAgentNode(const std::shared_ptr<ResponseNode>& response_node);
  void initializeConfigurationChecksums(const std::shared_ptr<ResponseNode>& response_node);
  void initializeFlowMonitor(const std::shared_ptr<ResponseNode>& response_node, core::ProcessGroup* root);

  mutable std::mutex component_metrics_mutex_;
  std::unordered_map<std::string, std::shared_ptr<ResponseNode>> component_metrics_;
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<core::Repository> provenance_repo_;
  std::shared_ptr<core::Repository> flow_file_repo_;
  core::FlowConfiguration* flow_configuration_ = nullptr;
  core::controller::ControllerServiceProvider* controller_ = nullptr;
  state::StateMonitor* update_sink_ = nullptr;
  std::mutex callback_mutex_;
  std::map<utils::Identifier, std::function<void(core::ProcessGroup*)>> flow_change_callbacks_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<ResponseNodeLoader>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::state::response
