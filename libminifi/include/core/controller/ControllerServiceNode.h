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
#ifndef LIBMINIFI_INCLUDE_CORE_CONTROLLER_CONTROLLERSERVICENODE_H_
#define LIBMINIFI_INCLUDE_CORE_CONTROLLER_CONTROLLERSERVICENODE_H_

#include "core/Core.h"
#include "core/ConfigurableComponent.h"
#include "core/logging/Logger.h"
#include "properties/Configure.h"
#include "ControllerService.h"
#include "io/validation.h"
#include "Exception.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace controller {

class ControllerServiceNode : public CoreComponent, public ConfigurableComponent {
 public:

  /**
   * Constructor for the controller service node.
   * @param service controller service reference
   * @param id identifier for this node.
   * @param configuration shared pointer configuration.
   */
  explicit ControllerServiceNode(std::shared_ptr<ControllerService> service, const std::string &id, std::shared_ptr<Configure> configuration)
      : CoreComponent(id),
        ConfigurableComponent(),
        active(false),
        configuration_(configuration),
        controller_service_(service){
    if (service == nullptr || IsNullOrEmpty(service.get())) {
      throw Exception(GENERAL_EXCEPTION, "Service must be properly configured");
    }
    if (IsNullOrEmpty(configuration)) {
      throw Exception(GENERAL_EXCEPTION, "Configuration must be properly configured");
    }
    service->setConfiguration(configuration);
  }

  virtual void initialize() {
    controller_service_->initialize();
    // set base supported properties
    Property property("Linked Services", "Referenced Controller Services");
    std::set<Property> supportedProperties;
    supportedProperties.insert(property);
    setSupportedProperties(supportedProperties);
  }
  void setName(const std::string name) {
    CoreComponent::setName(name);
    controller_service_->setName(name);
  }

  void setUUID(utils::Identifier &uuid) {
    CoreComponent::setUUID(uuid);
    controller_service_->setUUID(uuid);
  }

  /**
   * Returns the implementation of the Controller Service that this ControllerServiceNode
   * maintains
   * @return the implementation of the Controller Service
   */
  std::shared_ptr<ControllerService> &getControllerServiceImplementation();
  std::vector<std::shared_ptr<ControllerServiceNode> > &getLinkedControllerServices();
  std::vector<std::shared_ptr<ConfigurableComponent> > &getLinkedComponents();

  /**
   * Returns true if we can be enabled.
   * Returns false if this ControllerServiceNode cannot be enabled.
   */
  virtual bool canEnable()=0;

  virtual bool enabled() {
    return active.load();
  }

  /**
   * Function to enable the controller service node.
   */
  virtual bool enable() = 0;

  /**
   * Function to disable the controller service node.
   */
  virtual bool disable() = 0;

  virtual bool supportsDynamicProperties() {
    return false;
  }

  ControllerServiceNode(const ControllerServiceNode &other) = delete;
  ControllerServiceNode &operator=(const ControllerServiceNode &parent) = delete;
 protected:

  bool canEdit() {
    return true;
  }

  std::atomic<bool> active;
  std::shared_ptr<Configure> configuration_;
  // controller service.
  std::shared_ptr<ControllerService> controller_service_;
  // linked controller services.
  std::vector<std::shared_ptr<ControllerServiceNode> > linked_controller_services_;
  std::vector<std::shared_ptr<ConfigurableComponent> > linked_components_;
};

} /* namespace controller */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_CONTROLLER_CONTROLLERSERVICENODE_H_ */
