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
#include "core/ConfigurableComponent.h"
#include "core/logging/Logger.h"
#include "minifi-cpp/core/controller/ControllerServiceNode.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "properties/Configure.h"
#include "minifi-cpp/core/controller/ControllerService.h"
#include "io/validation.h"
#include "Exception.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceNodeImpl : public CoreComponentImpl, public ConfigurableComponentImpl, public virtual ControllerServiceNode {
 public:
  /**
   * Constructor for the controller service node.
   * @param service controller service reference
   * @param id identifier for this node.
   * @param configuration shared pointer configuration.
   */
  explicit ControllerServiceNodeImpl(std::shared_ptr<ControllerService> service, std::string id, std::shared_ptr<Configure> configuration)
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
  std::shared_ptr<ControllerService> getControllerServiceImplementation() override;
  const ControllerService* getControllerServiceImplementation() const override;
  const std::vector<ControllerServiceNode*>& getLinkedControllerServices() const override;

  bool enabled() override {
    return active.load();
  }

  bool supportsDynamicProperties() const override {
    return false;
  }

  bool supportsDynamicRelationships() const override {
    return false;
  }

  ControllerServiceNodeImpl(const ControllerServiceNodeImpl &other) = delete;
  ControllerServiceNodeImpl &operator=(const ControllerServiceNodeImpl &parent) = delete;

 protected:
  bool canEdit() override {
    return true;
  }

  std::atomic<bool> active;
  std::shared_ptr<Configure> configuration_;
  // controller service.
  std::shared_ptr<ControllerService> controller_service_;
  // linked controller services.
  std::vector<ControllerServiceNode*> linked_controller_services_;
};

}  // namespace org::apache::nifi::minifi::core::controller
