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
#include <string>
#include <utility>
#include <vector>

#include "core/Core.h"
#include "core/ConfigurableComponentImpl.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "properties/Configure.h"
#include "core/controller/ControllerService.h"
#include "io/validation.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceNode : public CoreComponentImpl, public ConfigurableComponentImpl {
 public:
  explicit ControllerServiceNode(std::shared_ptr<ControllerService> service, std::string id, std::shared_ptr<Configure> configuration)
      : CoreComponentImpl(std::move(id)),
        active(false),
        configuration_(std::move(configuration)),
        controller_service_(std::move(service)) {
    if (controller_service_ == nullptr || IsNullOrEmpty(controller_service_.get())) {
      throw Exception(GENERAL_EXCEPTION, "Service must be properly configured");
    }
    if (IsNullOrEmpty(configuration_)) {
      throw Exception(GENERAL_EXCEPTION, "Configuration must be properly configured");
    }
    controller_service_->setConfiguration(configuration_);
  }

  void initialize() override {
    controller_service_->initialize();
    setSupportedProperties(std::array<PropertyReference, 1>{
      PropertyDefinitionBuilder<>::createProperty("Linked Services").withDescription("Referenced Controller Services").build()
    });
  }

  void setName(std::string name) override {
    controller_service_->setName(name);
    CoreComponentImpl::setName(std::move(name));
  }

  void setUUID(const utils::Identifier& uuid) override {
    CoreComponentImpl::setUUID(uuid);
    controller_service_->setUUID(uuid);
  }

  /**
   * Returns the implementation of the Controller Service that this ControllerServiceNode
   * maintains
   * @return the implementation of the Controller Service
   */
  std::shared_ptr<ControllerService> getControllerServiceImplementation() const;
  template<typename T>
  std::shared_ptr<T> getControllerServiceImplementation() {
    if (auto impl = getControllerServiceImplementation()) {
      return {impl, impl->getImplementation<T>()};
    }
    return {};
  }
  virtual const std::vector<ControllerServiceNode*>& getLinkedControllerServices() const;

  virtual bool enabled() {
    return active.load();
  }

  virtual bool canEnable() = 0;
  virtual bool enable() = 0;
  virtual bool disable() = 0;

  bool supportsDynamicProperties() const override {
    return false;
  }

  bool supportsDynamicRelationships() const override {
    return false;
  }

  ControllerServiceNode(const ControllerServiceNode &other) = delete;
  ControllerServiceNode &operator=(const ControllerServiceNode &parent) = delete;

 protected:
  bool canEdit() override {
    return true;
  }

  std::atomic<bool> active;
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<ControllerService> controller_service_;
  std::vector<ControllerServiceNode*> linked_controller_services_;
};

}  // namespace org::apache::nifi::minifi::core::controller
