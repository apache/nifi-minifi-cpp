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

/**
 * Purpose: Controller service map is the mapping between service names
 * and ControllerService Nodes.
 * Justification: This abstracts the map, the controller for the map, and the
 * accounting into an object that will be used amongst the separate Controller
 * Service classes. This will help avoid help when sending the map as a reference.
 */
class ControllerServiceMap {
 public:
  ControllerServiceMap() = default;
  ~ControllerServiceMap() = default;
  ControllerServiceMap(const ControllerServiceMap&) = delete;
  ControllerServiceMap& operator=(const ControllerServiceMap&) = delete;
  ControllerServiceMap(ControllerServiceMap&&) = delete;
  ControllerServiceMap& operator=(ControllerServiceMap&&) = delete;

  /**
   * Gets the controller service node using the <code>id</code>
   * @param id identifier for controller service.
   * @return nullptr if node does not exist or controller service node shared pointer.
   */
  std::shared_ptr<ControllerServiceNode> getControllerServiceNode(const std::string &id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto exists = controller_services_.find(id);
    if (exists != controller_services_.end())
      return exists->second;
    else
      return nullptr;
  }

  /**
   * Removes the controller service.
   * @param serviceNode service node to remove
   *
   */
  bool removeControllerService(const std::shared_ptr<ControllerServiceNode> &serviceNode) {
    if (serviceNode == nullptr || serviceNode == nullptr)
      return false;
    std::lock_guard<std::mutex> lock(mutex_);
    controller_services_[serviceNode->getName()] = nullptr;
    controller_services_list_.erase(serviceNode);
    return true;
  }

  /**
   * Puts the service node into the mapping using <code>id</code> as the identifier
   * @param id service identifier
   * @param serviceNode controller service node shared pointer.
   *
   */
  bool put(const std::string &id, const std::shared_ptr<ControllerServiceNode> &serviceNode) {
    if (id.empty() || serviceNode == nullptr || serviceNode == nullptr)
      return false;
    std::lock_guard<std::mutex> lock(mutex_);
    controller_services_[id] = serviceNode;
    controller_services_list_.insert(serviceNode);
    return true;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &node : controller_services_list_) {
      node->disable();
    }
    controller_services_.clear();
    controller_services_list_.clear();
  }

  /**
   * Gets all controller services.
   * @return controller service node shared pointers.
   */
  std::vector<std::shared_ptr<ControllerServiceNode>> getAllControllerServices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {controller_services_list_.begin(), controller_services_list_.end()};
  }

 protected:
  mutable std::mutex mutex_;
  std::set<std::shared_ptr<ControllerServiceNode>> controller_services_list_;
  std::map<std::string, std::shared_ptr<ControllerServiceNode>> controller_services_;
};

}  // namespace org::apache::nifi::minifi::core::controller
