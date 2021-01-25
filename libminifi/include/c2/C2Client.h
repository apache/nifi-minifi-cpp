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

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <mutex>
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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

class C2Client : public core::Flow, public state::response::NodeReporter {
 public:
  C2Client(
      std::shared_ptr<Configure> configuration, std::shared_ptr<core::Repository> provenance_repo,
      std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<core::ContentRepository> content_repo,
      std::unique_ptr<core::FlowConfiguration> flow_configuration, std::shared_ptr<utils::file::FileSystem> filesystem,
      std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<C2Client>::getLogger());

  void initialize(core::controller::ControllerServiceProvider *controller, state::Pausable *pause_handler, const std::shared_ptr<state::StateMonitor> &update_sink);

  std::shared_ptr<state::response::ResponseNode> getMetricsNode(const std::string& metrics_class) const override;

  std::vector<std::shared_ptr<state::response::ResponseNode>> getHeartbeatNodes(bool include_manifest) const override;

  void stopC2();

 protected:
  bool isC2Enabled() const;
  utils::optional<std::string> fetchFlow(const std::string& uri) const;

 private:
  void initializeComponentMetrics();
  void loadC2ResponseConfiguration(const std::string &prefix);
  std::shared_ptr<state::response::ResponseNode> loadC2ResponseConfiguration(const std::string &prefix, std::shared_ptr<state::response::ResponseNode> prev_node);

 protected:
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<utils::file::FileSystem> filesystem_;

 private:
  std::unique_ptr<C2Agent> c2_agent_;
  std::atomic_bool initialized_{false};
  std::shared_ptr<logging::Logger> logger_;

  mutable std::mutex metrics_mutex_;
  std::map<std::string, std::shared_ptr<state::response::ResponseNode>> root_response_nodes_;
  std::map<std::string, std::shared_ptr<state::response::ResponseNode>> component_metrics_;

 protected:
  std::atomic<bool> flow_update_{false};
};

}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
