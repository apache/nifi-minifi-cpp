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
#ifndef LIBMINIFI_INCLUDE_CORE_CONTROLLER_STANDARDStandardControllerServiceProvider_H_
#define LIBMINIFI_INCLUDE_CORE_CONTROLLER_STANDARDStandardControllerServiceProvider_H_

#include <iostream>
#include <memory>
#include <vector>
#include "core/ProcessGroup.h"
#include "SchedulingAgent.h"
#include "core/ClassLoader.h"
#include "ControllerService.h"
#include "ControllerServiceMap.h"
#include "ControllerServiceNode.h"
#include "StandardControllerServiceNode.h"
#include "ControllerServiceProvider.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace controller {

class StandardControllerServiceProvider : public ControllerServiceProvider,
    public std::enable_shared_from_this<StandardControllerServiceProvider> {
 public:

  explicit StandardControllerServiceProvider(
      std::shared_ptr<ControllerServiceMap> services,
      std::shared_ptr<ProcessGroup> root_group, std::shared_ptr<Configure> configuration,
      std::shared_ptr<minifi::SchedulingAgent> agent, ClassLoader &loader =
          ClassLoader::getDefaultClassLoader())
      : ControllerServiceProvider(services),
        root_group_(root_group),
        agent_(agent),
        extension_loader_(loader),
        configuration_(configuration),
        logger_(logging::LoggerFactory<StandardControllerServiceProvider>::getLogger()) {
  }

  explicit StandardControllerServiceProvider(
      std::shared_ptr<ControllerServiceMap> services,
      std::shared_ptr<ProcessGroup> root_group, std::shared_ptr<Configure> configuration,
      ClassLoader &loader = ClassLoader::getDefaultClassLoader())
      : ControllerServiceProvider(services),
        root_group_(root_group),
        agent_(0),
        extension_loader_(loader),
        configuration_(configuration),
        logger_(logging::LoggerFactory<StandardControllerServiceProvider>::getLogger()) {
  }

  explicit StandardControllerServiceProvider(
      const StandardControllerServiceProvider && other)
      : ControllerServiceProvider(std::move(other)),
        root_group_(std::move(other.root_group_)),
        agent_(std::move(other.agent_)),
        extension_loader_(other.extension_loader_),
        configuration_(other.configuration_),
        logger_(logging::LoggerFactory<StandardControllerServiceProvider>::getLogger()) {

  }

  void setRootGroup(std::shared_ptr<ProcessGroup> rg) {
    root_group_ = rg;
  }

  void setSchedulingAgent(std::shared_ptr<minifi::SchedulingAgent> agent) {
    agent_ = agent;
  }

  std::shared_ptr<ControllerServiceNode> createControllerService(
      const std::string &type, const std::string &id,
      bool firstTimeAdded) {

    std::shared_ptr<ControllerService> new_controller_service =
        extension_loader_.instantiate<ControllerService>(type, id);

    if (nullptr == new_controller_service) {
      return nullptr;
    }

    std::shared_ptr<ControllerServiceNode> new_service_node = std::make_shared<
        StandardControllerServiceNode>(
        new_controller_service,
        std::static_pointer_cast<ControllerServiceProvider>(shared_from_this()),
        id, configuration_);

    controller_map_->put(id, new_service_node);
    return new_service_node;

  }


  void enableControllerService(
      std::shared_ptr<ControllerServiceNode> &serviceNode) {
    if (serviceNode->canEnable()) {
      agent_->enableControllerService(serviceNode);
    }
  }


  virtual void enableAllControllerServices() {
    logger_->log_info("Enabling %d controller services",
                      controller_map_->getAllControllerServices().size());
    for (auto service : controller_map_->getAllControllerServices()) {

      if (service->canEnable()) {
        logger_->log_info("Enabling %s", service->getName());
        agent_->enableControllerService(service);
      } else {
        logger_->log_info("Could not enable %s", service->getName());
      }
    }
  }


  void enableControllerServices(
      std::vector<std::shared_ptr<ControllerServiceNode>> serviceNodes) {
    for (auto node : serviceNodes) {
      enableControllerService(node);
    }
  }


  void disableControllerService(
      std::shared_ptr<ControllerServiceNode> &serviceNode) {
    if (!IsNullOrEmpty(serviceNode.get()) && serviceNode->enabled()) {
      agent_->disableControllerService(serviceNode);
    }
  }


  void verifyCanStopReferencingComponents(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  }


  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> unscheduleReferencingComponents(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
    std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> references =
        findLinkedComponents(serviceNode);
    for (auto ref : references) {
      agent_->disableControllerService(ref);
    }
    return references;
  }


  void verifyCanDisableReferencingServices(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
    std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> references =
        findLinkedComponents(serviceNode);
    for (auto ref : references) {
      if (!ref->canEnable()) {
        logger_->log_info("Cannot disable %s", ref->getName());
      }
    }
  }


  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> disableReferencingServices(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
    std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> references =
        findLinkedComponents(serviceNode);
    for (auto ref : references) {
      agent_->disableControllerService(ref);
    }

    return references;
  }

  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> enableReferencingServices(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
    std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> references =
        findLinkedComponents(serviceNode);
    for (auto ref : references) {
      agent_->enableControllerService(ref);
    }
    return references;
  }

  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> scheduleReferencingComponents(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
    std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> references =
        findLinkedComponents(serviceNode);
    for (auto ref : references) {
      agent_->enableControllerService(ref);
    }
    return references;
  }

 protected:

  bool canEdit() {
    return false;
  }

  std::shared_ptr<minifi::SchedulingAgent> agent_;

  ClassLoader &extension_loader_;

  std::shared_ptr<Configure> configuration_;

  std::shared_ptr<ProcessGroup> root_group_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace controller */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_CONTROLLER_STANDARDStandardControllerServiceProvider_H_ */
