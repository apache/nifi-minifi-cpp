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
#include <set>
#include <vector>
#include <map>
#include <string>
#include "ControllerServiceNode.h"
#include "io/validation.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceNodeMap {
 public:
  ControllerServiceNodeMap() = default;
  ~ControllerServiceNodeMap() = default;
  ControllerServiceNodeMap(const ControllerServiceNodeMap&) = delete;
  ControllerServiceNodeMap& operator=(const ControllerServiceNodeMap&) = delete;
  ControllerServiceNodeMap(ControllerServiceNodeMap&&) = delete;
  ControllerServiceNodeMap& operator=(ControllerServiceNodeMap&&) = delete;

  ControllerServiceNode* get(const std::string &id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto exists = controller_service_nodes_.find(id);
    if (exists != controller_service_nodes_.end())
      return exists->second.get();
    else
      return nullptr;
  }

  bool put(const std::string &id, const std::shared_ptr<ControllerServiceNode> &serviceNode) {
    if (id.empty() || serviceNode == nullptr)
      return false;
    std::lock_guard<std::mutex> lock(mutex_);
    controller_service_nodes_[id] = serviceNode;
    return true;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, node] : controller_service_nodes_) {
      node->disable();
    }
    controller_service_nodes_.clear();
  }

  std::vector<std::shared_ptr<ControllerServiceNode>> getAllControllerServices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<ControllerServiceNode>> services;
    services.reserve(controller_service_nodes_.size());
    for (const auto& [id, node] : controller_service_nodes_) {
      services.push_back(node);
    }
    return services;
  }

 protected:
  mutable std::mutex mutex_;
  std::map<std::string, std::shared_ptr<ControllerServiceNode>> controller_service_nodes_;
};

}  // namespace org::apache::nifi::minifi::core::controller
