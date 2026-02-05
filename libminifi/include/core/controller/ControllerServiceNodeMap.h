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

namespace org::apache::nifi::minifi::core {

class ProcessGroup;

namespace controller {

class ControllerServiceNodeMap {
 public:
  ControllerServiceNodeMap() = default;
  ~ControllerServiceNodeMap() = default;
  ControllerServiceNodeMap(const ControllerServiceNodeMap&) = delete;
  ControllerServiceNodeMap& operator=(const ControllerServiceNodeMap&) = delete;
  ControllerServiceNodeMap(ControllerServiceNodeMap&&) = delete;
  ControllerServiceNodeMap& operator=(ControllerServiceNodeMap&&) = delete;

  ControllerServiceNode* get(const std::string &id) const;
  ControllerServiceNode* get(const std::string &id, const utils::Identifier &processor_or_controller_uuid) const;

  bool put(std::string id, std::shared_ptr<ControllerServiceNode> controller_service_node, ProcessGroup* parent_group);

  bool register_alternative_key(std::string primary_key, std::string alternative_key);

  void clear();
  std::vector<std::shared_ptr<ControllerServiceNode>> getAllControllerServices() const;

 protected:
  mutable std::mutex mutex_;

  struct ServiceEntry {
    std::shared_ptr<ControllerServiceNode> controller_service_node;
    ProcessGroup* parent_group;
  };

  const ServiceEntry* getEntry(std::string_view primary_key, const std::scoped_lock<std::mutex>& mutex) const;

  std::map<std::string, ServiceEntry, std::less<>> services_;
  std::map<std::string, std::string, std::less<>> alternative_keys;
};

}  // namespace controller
}  // namespace org::apache::nifi::minifi::core
