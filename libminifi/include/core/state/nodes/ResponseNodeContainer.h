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

#include "ResponseNodeLoader.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::state::response {

class ResponseNodeContainer {
 public:
  ResponseNodeContainer(ResponseNodeLoader& response_node_loader) : response_node_loader_(response_node_loader) {}
  void addRootResponseNode(const std::string& name, const std::shared_ptr<ResponseNode>& response_node);
  std::unordered_map<std::string, std::shared_ptr<ResponseNode>> getRootResponseNodes() const;
  void updateResponseNodeConnections(core::ProcessGroup* root);
  std::shared_ptr<ResponseNode> getRootResponseNode(const std::string& name) const;

 private:
  void updateResponseNodeConnections(const std::shared_ptr<ResponseNode>& response_node, std::map<std::string, Connection*> connections);

  mutable std::mutex response_node_mutex_;
  std::unordered_map<std::string, std::shared_ptr<ResponseNode>> root_response_nodes_;
  ResponseNodeLoader& response_node_loader_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<ResponseNodeContainer>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::state::response
