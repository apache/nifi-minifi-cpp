// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/controller/StandardControllerServiceNode.h"
#include <memory>
#include <mutex>
#include <algorithm>

namespace org::apache::nifi::minifi::core::controller {

bool StandardControllerServiceNode::canEnable() {
  if (active) {
    return false;
  }

  return std::all_of(linked_controller_services_.begin(), linked_controller_services_.end(), [](auto linked_service) {
    return linked_service->canEnable();
  });
}

bool StandardControllerServiceNode::enable() {
  logger_->log_trace("Enabling CSN {}", getName());
  if (active) {
    logger_->log_debug("CSN {} is already enabled", getName());
    return true;
  }
  if (auto linked_services = getAllPropertyValues("Linked Services")) {
    for (const auto& linked_service : *linked_services) {
      ControllerServiceNode* csNode = provider->getControllerServiceNode(linked_service, controller_service_->getUUID());
      if (nullptr != csNode) {
        std::lock_guard<std::mutex> lock(mutex_);
        linked_controller_services_.push_back(csNode);
      }
    }
  }
  std::shared_ptr<ControllerService> impl = getControllerServiceImplementation();
  if (nullptr == impl) {
    logger_->log_warn("Service '{}' service implementation could not be found", controller_service_->getName());
    controller_service_->setState(ENABLING);
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::shared_ptr<ControllerService>> services;
  std::vector<ControllerServiceNode*> service_nodes;
  services.reserve(linked_controller_services_.size());
  for (const auto& service : linked_controller_services_) {
    services.push_back(service->getControllerServiceImplementation());
    if (!service->enable()) {
      logger_->log_warn("Linked Service '{}' could not be enabled", service->getName());
      return false;
    }
  }

  try {
    impl->setLinkedControllerServices(services);
    impl->onEnable();
  } catch(const std::exception& e) {
    logger_->log_warn("Service '{}' failed to enable: {}", getName(), e.what());
    controller_service_->setState(ENABLING);
    return false;
  }

  active = true;
  controller_service_->setState(ENABLED);
  return true;
}

bool StandardControllerServiceNode::disable() {
  controller_service_->setState(DISABLED);
  active = false;
  return true;
}

}  // namespace org::apache::nifi::minifi::core::controller
