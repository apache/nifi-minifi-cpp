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
#include <vector>
#include <string>

#include "ControllerServiceProvider.h"
#include "ControllerServiceNode.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace controller {

class ForwardingControllerServiceProvider : public ControllerServiceProvider {
 public:
  using ControllerServiceProvider::ControllerServiceProvider;

  std::shared_ptr<ControllerServiceNode> createControllerService(const std::string &type, const std::string &longType, const std::string &id, bool firstTimeAdded) override {
    return controller_service_provider_impl_->createControllerService(type, longType, id, firstTimeAdded);
  }

  std::shared_ptr<ControllerServiceNode> getControllerServiceNode(const std::string &id) const override {
    return controller_service_provider_impl_->getControllerServiceNode(id);
  }

  void removeControllerService(const std::shared_ptr<ControllerServiceNode> &serviceNode) override {
    return controller_service_provider_impl_->removeControllerService(serviceNode);
  }

  std::future<utils::TaskRescheduleInfo> enableControllerService(std::shared_ptr<ControllerServiceNode> &serviceNode) override {
    return controller_service_provider_impl_->enableControllerService(serviceNode);
  }

  void enableControllerServices(std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> serviceNodes) override {
    return controller_service_provider_impl_->enableControllerServices(serviceNodes);
  }

  std::future<utils::TaskRescheduleInfo> disableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override {
    return controller_service_provider_impl_->disableControllerService(serviceNode);
  }

  void clearControllerServices() override {
    return controller_service_provider_impl_->clearControllerServices();
  }

  std::shared_ptr<ControllerService> getControllerService(const std::string &identifier) const override {
    return controller_service_provider_impl_->getControllerService(identifier);
  }

  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> getAllControllerServices() override {
    return controller_service_provider_impl_->getAllControllerServices();
  }

  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> unscheduleReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override {
    return controller_service_provider_impl_->unscheduleReferencingComponents(serviceNode);
  }

  void verifyCanEnableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override {
    return controller_service_provider_impl_->verifyCanEnableReferencingServices(serviceNode);
  }

  void verifyCanDisableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override {
    return controller_service_provider_impl_->verifyCanDisableReferencingServices(serviceNode);
  }

  void verifyCanStopReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override {
    return controller_service_provider_impl_->verifyCanStopReferencingComponents(serviceNode);
  }

  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> disableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override {
    return controller_service_provider_impl_->disableReferencingServices(serviceNode);
  }

  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> enableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override {
    return controller_service_provider_impl_->enableReferencingServices(serviceNode);
  }

  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> scheduleReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override {
    return controller_service_provider_impl_->scheduleReferencingComponents(serviceNode);
  }

  std::shared_ptr<ControllerService> getControllerServiceForComponent(const std::string &serviceIdentifier, const utils::Identifier &componentId) const override {
    return controller_service_provider_impl_->getControllerServiceForComponent(serviceIdentifier, componentId);
  }

  bool isControllerServiceEnabled(const std::string &identifier) override {
    return controller_service_provider_impl_->isControllerServiceEnabled(identifier);
  }

  bool isControllerServiceEnabling(const std::string &identifier) override {
    return controller_service_provider_impl_->isControllerServiceEnabling(identifier);
  }

  const std::string getControllerServiceName(const std::string &identifier) const override {
    return controller_service_provider_impl_->getControllerServiceName(identifier);
  }

  void enableAllControllerServices() override {
    return controller_service_provider_impl_->enableAllControllerServices();
  }

  void disableAllControllerServices() override {
    return controller_service_provider_impl_->disableAllControllerServices();
  }

 protected:
  std::shared_ptr<ControllerServiceProvider> controller_service_provider_impl_;
};

}  // namespace controller
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
