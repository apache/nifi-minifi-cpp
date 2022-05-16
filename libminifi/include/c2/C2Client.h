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

#include <map>
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
#include "core/state/ConnectionMonitor.h"
#include "core/state/nodes/ResponseNodeManager.h"

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

  std::shared_ptr<state::response::ResponseNode> getMetricsNode(const std::string& metrics_class) const override;

  std::vector<std::shared_ptr<state::response::ResponseNode>> getHeartbeatNodes(bool include_manifest) const override;

  void stopC2();

 protected:
  bool isC2Enabled() const;
  std::optional<std::string> fetchFlow(const std::string& uri) const;

 private:
  void loadC2ResponseConfiguration(const std::string &prefix);
  std::shared_ptr<state::response::ResponseNode> loadC2ResponseConfiguration(const std::string &prefix, std::shared_ptr<state::response::ResponseNode> prev_node);

 protected:
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<utils::file::FileSystem> filesystem_;

 private:
  std::unique_ptr<C2Agent> c2_agent_;
  std::mutex initialization_mutex_;
  bool initialized_ = false;
  std::shared_ptr<core::logging::Logger> logger_;

  mutable std::mutex metrics_mutex_;
  std::map<std::string, std::shared_ptr<state::response::ResponseNode>> root_response_nodes_;

 protected:
  std::atomic<bool> flow_update_{false};
  std::function<void()> request_restart_;
  state::response::ResponseNodeManager response_node_manager_;
};

}  // namespace org::apache::nifi::minifi::c2
