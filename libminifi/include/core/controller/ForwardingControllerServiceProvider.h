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

namespace org::apache::nifi::minifi::core::controller {

class ForwardingControllerServiceProvider : public ControllerServiceProviderImpl {
 public:
  using ControllerServiceProviderImpl::ControllerServiceProviderImpl;

  std::shared_ptr<ControllerServiceNode> createControllerService(const std::string &type, const std::string &longType, const std::string &id, bool firstTimeAdded) override {
    return controller_service_provider_impl_->createControllerService(type, longType, id, firstTimeAdded);
  }

  ControllerServiceNode* getControllerServiceNode(const std::string &id) const override {
    return controller_service_provider_impl_->getControllerServiceNode(id);
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

}  // namespace org::apache::nifi::minifi::core::controller
