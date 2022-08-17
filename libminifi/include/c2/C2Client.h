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
#include <unordered_set>

#include "c2/C2Agent.h"
#include "core/controller/ControllerServiceProvider.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/state/nodes/MetricsBase.h"
#include "core/Repository.h"
#include "core/ContentRepository.h"
#include "core/ProcessGroup.h"
#include "core/Flow.h"
#include "utils/file/FileSystem.h"
#include "core/state/nodes/ResponseNodeLoader.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::c2 {

class C2Client : public core::Flow, public state::response::NodeReporter {
 public:
  C2Client(
      std::shared_ptr<Configure> configuration, std::shared_ptr<core::Repository> provenance_repo,
      std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<core::ContentRepository> content_repo,
      std::unique_ptr<core::FlowConfiguration> flow_configuration, std::shared_ptr<utils::file::FileSystem> filesystem,
      std::function<void()> request_restart,
      std::shared_ptr<core::logging::Logger> logger = core::logging::LoggerFactory<C2Client>::getLogger());

  void initialize(core::controller::ControllerServiceProvider *controller, state::Pausable *pause_handler, state::StateMonitor* update_sink);
  std::optional<state::response::NodeReporter::ReportedNode> getMetricsNode(const std::string& metrics_class) const override;
  std::vector<state::response::NodeReporter::ReportedNode> getHeartbeatNodes(bool include_manifest) const override;

  void stopC2();
  void initializeResponseNodes(core::ProcessGroup* root);
  void clearResponseNodes();

 protected:
  bool isC2Enabled() const;
  std::optional<std::string> fetchFlow(const std::string& uri) const;

 private:
  void loadC2ResponseConfiguration(const std::string &prefix);
  std::shared_ptr<state::response::ResponseNode> loadC2ResponseConfiguration(const std::string &prefix, std::shared_ptr<state::response::ResponseNode> prev_node);
  void loadNodeClasses(const std::string& class_definitions, const std::shared_ptr<state::response::ResponseNode>& new_node);

 protected:
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<utils::file::FileSystem> filesystem_;

 private:
  std::unique_ptr<C2Agent> c2_agent_;
  std::mutex initialization_mutex_;
  bool initialized_ = false;
  std::shared_ptr<core::logging::Logger> logger_;
  mutable std::mutex metrics_mutex_;

  // Name and response node value of the root response nodes defined in nifi.c2.root.classes and nifi.c2.root.class.definitions
  // In case a root class is defined to be a processor metric there can be multiple response nodes if the same processor is defined
  // multiple times in the flow
  std::unordered_map<std::string, std::vector<std::shared_ptr<state::response::ResponseNode>>> root_response_nodes_;

 protected:
  std::atomic<bool> flow_update_{false};
  std::function<void()> request_restart_;
  state::response::ResponseNodeLoader response_node_loader_;
};

}  // namespace org::apache::nifi::minifi::c2
