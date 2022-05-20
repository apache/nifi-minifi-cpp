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
#include "core/state/nodes/ResponseNodeContainer.h"
#include "core/state/ConnectionMonitor.h"

namespace org::apache::nifi::minifi::state::response {

void ResponseNodeContainer::addRootResponseNode(const std::string& name, const std::shared_ptr<ResponseNode>& response_node) {
  std::lock_guard<std::mutex> lock(response_node_mutex_);
  root_response_nodes_.emplace(name, response_node);
}

std::unordered_map<std::string, std::shared_ptr<ResponseNode>> ResponseNodeContainer::getRootResponseNodes() const {
  std::lock_guard<std::mutex> lock(response_node_mutex_);
  return root_response_nodes_;
}

void ResponseNodeContainer::updateResponseNodeConnections(core::ProcessGroup* root) {
  if (!root) {
    return;
  }

  std::map<std::string, Connection*> connections;
  root->getConnections(connections);

  std::lock_guard<std::mutex> lock(response_node_mutex_);
  for (const auto& [_, response_node] : root_response_nodes_) {
    updateResponseNodeConnections(response_node, connections);
  }
}

void ResponseNodeContainer::updateResponseNodeConnections(const std::shared_ptr<ResponseNode>& response_node, std::map<std::string, Connection*> connections) {
  auto connection_monitor = dynamic_cast<ConnectionMonitor*>(response_node.get());
  if (connection_monitor != nullptr) {
    connection_monitor->clearConnections();
    for (const auto &con : connections) {
      connection_monitor->updateConnection(con.second);
    }
  }

  auto object_node = dynamic_cast<ObjectNode*>(response_node.get());
  if (object_node != nullptr) {
    for (const auto& child_node : object_node->get_child_nodes()) {
      updateResponseNodeConnections(child_node, connections);
    }
  }
}

std::shared_ptr<ResponseNode> ResponseNodeContainer::getRootResponseNode(const std::string& name) const {
  std::lock_guard<std::mutex> lock(response_node_mutex_);
  const auto iter = root_response_nodes_.find(name);
  if (iter != root_response_nodes_.end()) {
    return iter->second;
  }
  return nullptr;
}

}  // namespace org::apache::nifi::minifi::state::response
