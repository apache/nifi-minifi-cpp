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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace controller {
std::shared_ptr<core::ProcessGroup> &StandardControllerServiceNode::getProcessGroup() {
  std::lock_guard<std::mutex> lock(mutex_);
  return process_group_;
}

void StandardControllerServiceNode::setProcessGroup(std::shared_ptr<ProcessGroup> &processGroup) {
  std::lock_guard<std::mutex> lock(mutex_);
  process_group_ = processGroup;
}

bool StandardControllerServiceNode::enable() {
  Property property("Linked Services", "Referenced Controller Services");
  controller_service_->setState(ENABLED);
  logger_->log_trace("Enabling CSN %s", getName());
  if (getProperty(property.getName(), property)) {
    active = true;
    for (auto linked_service : property.getValues()) {
      std::shared_ptr<ControllerServiceNode> csNode = provider->getControllerServiceNode(linked_service);
      if (nullptr != csNode) {
        std::lock_guard<std::mutex> lock(mutex_);
        linked_controller_services_.push_back(csNode);
      }
    }
  } else {
  }
  std::shared_ptr<ControllerService> impl = getControllerServiceImplementation();
  if (nullptr != impl) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<ControllerService> > services;
    for (auto service : linked_controller_services_) {
      services.push_back(service->getControllerServiceImplementation());
    }
    impl->setLinkedControllerServices(services);
    impl->onEnable();
  }
  return true;
}

} /* namespace controller */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
