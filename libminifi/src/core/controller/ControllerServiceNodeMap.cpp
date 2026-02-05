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

#include <ranges>

#include "core/ProcessGroup.h"

namespace org::apache::nifi::minifi::core::controller {

ControllerServiceNode* ControllerServiceNodeMap::get(const std::string &id) const {
  const std::scoped_lock lock(mutex_);
  if (const auto entry = getEntry(id, lock)) {
    return entry->controller_service_node.get();
  }
  return nullptr;
}

ControllerServiceNode* ControllerServiceNodeMap::get(const std::string &id, const utils::Identifier& processor_or_controller_uuid) const {
  const std::scoped_lock lock(mutex_);
  const auto entry = getEntry(id, lock);
  if (!entry || !entry->parent_group) {
    return nullptr;
  }


  if (entry->parent_group->findProcessorById(processor_or_controller_uuid, ProcessGroup::Traverse::IncludeChildren)) {
    return entry->controller_service_node.get();
  }

  if (entry->parent_group->findControllerService(processor_or_controller_uuid.to_string(), ProcessGroup::Traverse::IncludeChildren)) {
    return entry->controller_service_node.get();
  }

  return nullptr;
}

bool ControllerServiceNodeMap::put(std::string id, std::shared_ptr<ControllerServiceNode> controller_service_node,
    ProcessGroup* parent_group) {
  if (id.empty() || controller_service_node == nullptr || alternative_keys.contains(id)) {
    return false;
  }
  std::scoped_lock lock(mutex_);
  auto [_it, success] = services_.emplace(std::move(id), ServiceEntry{.controller_service_node = std::move(controller_service_node), .parent_group = parent_group});
  return success;
}


void ControllerServiceNodeMap::clear() {
  std::scoped_lock lock(mutex_);
  for (const auto& node: services_ | std::views::values) {
    node.controller_service_node->disable();
  }
  services_.clear();
}

std::vector<std::shared_ptr<ControllerServiceNode>> ControllerServiceNodeMap::getAllControllerServices() const {
  std::scoped_lock lock(mutex_);
  std::vector<std::shared_ptr<ControllerServiceNode>> services;
  services.reserve(services_.size());
  for (const auto& [controller_service_node, _parent_group]: services_ | std::views::values) {
    services.push_back(controller_service_node);
  }
  return services;
}

const ControllerServiceNodeMap::ServiceEntry* ControllerServiceNodeMap::getEntry(const std::string_view key, const std::scoped_lock<std::mutex>&) const {
  const auto it = services_.find(key);
  if (it != services_.end()) {
    return &it->second;
  }
  const auto primary_key_it = alternative_keys.find(key);
  if (primary_key_it == alternative_keys.end()) {
    return nullptr;
  }
  const auto it_from_primary = services_.find(primary_key_it->second);
  gsl_Expects(it_from_primary != services_.end());
  return &it_from_primary->second;
}


bool ControllerServiceNodeMap::register_alternative_key(std::string primary_key, std::string alternative_key) {
  std::scoped_lock lock(mutex_);
  if (!services_.contains(primary_key)) {
    return false;
  }

  auto [_it, success] = alternative_keys.emplace(std::move(alternative_key), std::move(primary_key));
  return success;
}

}  // namespace org::apache::nifi::minifi::core::controller
