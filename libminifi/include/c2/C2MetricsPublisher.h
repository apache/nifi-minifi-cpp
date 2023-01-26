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
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <functional>

#include "c2/C2Agent.h"
#include "core/controller/ControllerServiceProvider.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/state/nodes/MetricsBase.h"
#include "core/ProcessGroup.h"
#include "core/Core.h"
#include "utils/file/FileSystem.h"
#include "core/state/nodes/ResponseNodeLoader.h"
#include "utils/Id.h"
#include "core/state/MetricsPublisher.h"

namespace org::apache::nifi::minifi::c2 {

class C2MetricsPublisher : public core::CoreComponent, public state::response::NodeReporter, public state::MetricsPublisher {
 public:
  explicit C2MetricsPublisher(const std::string &name, const utils::Identifier &uuid = {});

  MINIFIAPI static constexpr const char* Description = "Class that provides C2 metrics to the C2Agent";

  void startC2(core::controller::ControllerServiceProvider *controller, state::Pausable *pause_handler, state::StateMonitor* update_sink);
  std::optional<state::response::NodeReporter::ReportedNode> getMetricsNode(const std::string& metrics_class) const override;
  std::vector<state::response::NodeReporter::ReportedNode> getHeartbeatNodes(bool include_manifest) const override;

  void stopC2();

  /**
   * Retrieves the agent manifest to be sent as a response to C2 DESCRIBE manifest
   * @return the agent manifest response node
   */
  state::response::NodeReporter::ReportedNode getAgentManifest() override;

  void clearMetricNodes() override;
  void loadMetricNodes() override;

 private:
  void loadC2ResponseConfiguration(const std::string &prefix);
  std::shared_ptr<state::response::ResponseNode> loadC2ResponseConfiguration(const std::string &prefix, std::shared_ptr<state::response::ResponseNode> prev_node);
  void loadNodeClasses(const std::string& class_definitions, const std::shared_ptr<state::response::ResponseNode>& new_node);

  mutable std::mutex metrics_mutex_;

  // Name and response node value of the root response nodes defined in nifi.c2.root.classes and nifi.c2.root.class.definitions
  // In case a root class is defined to be a processor metric there can be multiple response nodes if the same processor is defined
  // multiple times in the flow
  std::unordered_map<std::string, std::vector<std::shared_ptr<state::response::ResponseNode>>> root_response_nodes_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<C2MetricsPublisher>::getLogger();
};

}  // namespace org::apache::nifi::minifi::c2
