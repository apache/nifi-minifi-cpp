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

class ForwardingControllerServiceProvider : public ControllerServiceProvider {
 public:
  using ControllerServiceProvider::ControllerServiceProvider;

  std::shared_ptr<ControllerServiceNode> createControllerService(const std::string &type, const std::string &id) override {
    return controller_service_provider_impl_->createControllerService(type, id);
  }

  ControllerServiceNode* getControllerServiceNode(const std::string &id) const override {
    return controller_service_provider_impl_->getControllerServiceNode(id);
  }

  ControllerServiceNode* getControllerServiceNode(const std::string &id, const utils::Identifier &controller_uuid) const override {
    return controller_service_provider_impl_->getControllerServiceNode(id, controller_uuid);
  }

  void clearControllerServices() override {
    return controller_service_provider_impl_->clearControllerServices();
  }

  std::shared_ptr<ControllerService> getControllerService(const std::string &identifier) const override {
    return controller_service_provider_impl_->getControllerService(identifier);
  }

  std::shared_ptr<ControllerService> getControllerService(const std::string &identifier, const utils::Identifier &processor_uuid) const override {
    return controller_service_provider_impl_->getControllerService(identifier, processor_uuid);
  }

  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> getAllControllerServices() override {
    return controller_service_provider_impl_->getAllControllerServices();
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
