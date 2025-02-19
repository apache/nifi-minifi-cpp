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

#include "core/controller/StandardControllerServiceNode.h"
#include <memory>
#include <mutex>

namespace org::apache::nifi::minifi::core::controller {

bool StandardControllerServiceNode::enable() {
  controller_service_->setState(ENABLED);
  logger_->log_trace("Enabling CSN {}", getName());
  if (auto linked_services = getAllPropertyValues("Linked Services")) {
    active = true;
    for (const auto& linked_service : *linked_services) {
      ControllerServiceNode* csNode = provider->getControllerServiceNode(linked_service, controller_service_->getUUID());
      if (nullptr != csNode) {
        std::lock_guard<std::mutex> lock(mutex_);
        linked_controller_services_.push_back(csNode);
      }
    }
  }
  std::shared_ptr<ControllerService> impl = getControllerServiceImplementation();
  if (nullptr != impl) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<ControllerService>> services;
    std::vector<ControllerServiceNode*> service_nodes;
    services.reserve(linked_controller_services_.size());
    for (const auto& service : linked_controller_services_) {
      services.push_back(service->getControllerServiceImplementation());
      if (!service->enable()) {
        logger_->log_debug("Linked Service '{}' could not be enabled", service->getName());
        return false;
      }
    }
    impl->setLinkedControllerServices(services);
    impl->onEnable();
  }
  active = true;
  controller_service_->setState(ENABLED);
  return true;
}

}  // namespace org::apache::nifi::minifi::core::controller
