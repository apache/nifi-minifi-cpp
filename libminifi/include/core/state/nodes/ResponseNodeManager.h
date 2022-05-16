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
#include <string>
#include <memory>
#include <mutex>
#include <unordered_set>

#include "MetricsBase.h"
#include "core/ProcessGroup.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/FlowConfiguration.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::state::response {

class ResponseNodeManager {
 public:
  ResponseNodeManager(std::shared_ptr<Configure> configuration, std::shared_ptr<core::Repository> provenance_repo,
    std::shared_ptr<core::Repository> flow_file_repo, core::FlowConfiguration* flow_configuration);
  std::shared_ptr<ResponseNode> loadResponseNode(const std::string& clazz, core::ProcessGroup* root);
  void updateResponseNodeConnections(core::ProcessGroup* root);
  std::shared_ptr<state::response::ResponseNode> getComponentMetricsNode(const std::string& metrics_class) const;
  void initializeComponentMetrics(core::ProcessGroup* root);

  void setControllerServiceProvider(core::controller::ControllerServiceProvider* controller) {
    controller_ = controller;
  }

  void setStateMonitor(state::StateMonitor* update_sink) {
    update_sink_ = update_sink;
  }

 private:
  mutable std::mutex metrics_mutex_;
  std::unordered_map<std::string, std::shared_ptr<ResponseNode>> component_metrics_;
  std::unordered_set<state::ConnectionMonitor*> connection_monitors_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<ResponseNodeManager>::getLogger()};

  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<core::Repository> provenance_repo_;
  std::shared_ptr<core::Repository> flow_file_repo_;
  core::FlowConfiguration* flow_configuration_ = nullptr;

  core::controller::ControllerServiceProvider* controller_ = nullptr;
  state::StateMonitor* update_sink_ = nullptr;
};

}  // namespace org::apache::nifi::minifi::state::response
