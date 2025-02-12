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

#include "core/controller/ControllerServiceNodeMap.h"
#include "core/ProcessGroup.h"

namespace org::apache::nifi::minifi::core::controller {

ControllerServiceNode* ControllerServiceNodeMap::get(const std::string &id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto exists = controller_service_nodes_.find(id);
  if (exists != controller_service_nodes_.end())
    return exists->second.get();
  else
    return nullptr;
}

ControllerServiceNode* ControllerServiceNodeMap::get(const std::string &id, const utils::Identifier& processor_or_controller_uuid) const {
  std::lock_guard<std::mutex> lock(mutex_);
  ControllerServiceNode* controller = nullptr;
  auto exists = controller_service_nodes_.find(id);
  if (exists != controller_service_nodes_.end()) {
    controller = exists->second.get();
  } else {
    return nullptr;
  }

  auto process_group_of_controller_exists = process_groups_.find(id);
  ProcessGroup* process_group = nullptr;
  if (process_group_of_controller_exists != process_groups_.end()) {
    process_group = process_group_of_controller_exists->second;
  } else {
    return nullptr;
  }

  if (process_group->findProcessorById(processor_or_controller_uuid, ProcessGroup::Traverse::IncludeChildren)) {
    return controller;
  }

  if (process_group->findControllerService(processor_or_controller_uuid.to_string(), ProcessGroup::Traverse::IncludeChildren)) {
    return controller;
  }

  return nullptr;
}

bool ControllerServiceNodeMap::put(const std::string &id, const std::shared_ptr<ControllerServiceNode> &serviceNode) {
  if (id.empty() || serviceNode == nullptr)
    return false;
  std::lock_guard<std::mutex> lock(mutex_);
  controller_service_nodes_[id] = serviceNode;
  return true;
}

bool ControllerServiceNodeMap::put(const std::string &id, ProcessGroup* process_group) {
  if (id.empty() || process_group == nullptr)
    return false;
  std::lock_guard<std::mutex> lock(mutex_);
  process_groups_.emplace(id, gsl::make_not_null(process_group));
  return true;
}

void ControllerServiceNodeMap::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& [id, node] : controller_service_nodes_) {
    node->disable();
  }
  controller_service_nodes_.clear();
  process_groups_.clear();
}

std::vector<std::shared_ptr<ControllerServiceNode>> ControllerServiceNodeMap::getAllControllerServices() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::shared_ptr<ControllerServiceNode>> services;
  services.reserve(controller_service_nodes_.size());
  for (const auto& [id, node] : controller_service_nodes_) {
    services.push_back(node);
  }
  return services;
}

}  // namespace org::apache::nifi::minifi::core::controller
