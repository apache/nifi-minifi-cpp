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
#ifndef LIBMINIFI_INCLUDE_CORE_CONTROLLER_CONTROLLERSERVICE_H_
#define LIBMINIFI_INCLUDE_CORE_CONTROLLER_CONTROLLERSERVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "properties/Configure.h"
#include "core/Core.h"
#include "core/ConfigurableComponent.h"
#include "core/Connectable.h"

#define ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES \
  bool supportsDynamicProperties() const override { return SupportsDynamicProperties; }

namespace org::apache::nifi::minifi::core::controller {

enum ControllerServiceState {
  /**
   * Controller Service is disabled and cannot be used.
   */
  DISABLED,
  /**
   * Controller Service is in the process of being disabled.
   */
  DISABLING,
  /**
   * Controller Service is being enabled.
   */
  ENABLING,
  /**
   * Controller Service is enabled.
   */
  ENABLED
};

/**
 * Controller Service base class that contains some pure virtual methods.
 *
 * Design: OnEnable is executed when the controller service is being enabled.
 * Note that keeping state here must be protected  in this function.
 */
class ControllerService : public ConfigurableComponent, public Connectable {
 public:
  ControllerService()
      : Connectable(core::className<ControllerService>()),
        configuration_(std::make_shared<Configure>()) {
    current_state_ = DISABLED;
  }

  explicit ControllerService(std::string_view name, const utils::Identifier &uuid)
      : Connectable(name, uuid),
        configuration_(std::make_shared<Configure>()) {
    current_state_ = DISABLED;
  }

  explicit ControllerService(std::string_view name)
      : Connectable(name),
        configuration_(std::make_shared<Configure>()) {
    current_state_ = DISABLED;
  }

  void initialize() override {
    current_state_ = ENABLED;
  }

  bool supportsDynamicRelationships() const final {
    return false;
  }

  ~ControllerService() override {
    notifyStop();
  }

  /**
   * Replaces the configuration object within the controller service.
   */
  void setConfiguration(const std::shared_ptr<Configure> &configuration) {
    configuration_ = configuration;
  }

  ControllerServiceState getState() const {
    return current_state_.load();
  }

  /**
   * Function is called when Controller Services are enabled and being run
   */
  virtual void onEnable() {
  }

  /**
   * Function is called when Controller Services are disabled
   */
  virtual void notifyStop() {
  }

  void setState(ControllerServiceState state) {
    current_state_ = state;
    if (state == DISABLED) {
      notifyStop();
    }
  }

  void setLinkedControllerServices(const std::vector<std::shared_ptr<controller::ControllerService>> &services) {
    linked_services_ = services;
  }

 protected:
  std::vector<std::shared_ptr<controller::ControllerService> > linked_services_;
  std::shared_ptr<Configure> configuration_;
  mutable std::atomic<ControllerServiceState> current_state_;
  bool canEdit() override {
    return true;
  }
};

}  // namespace org::apache::nifi::minifi::core::controller

#endif  // LIBMINIFI_INCLUDE_CORE_CONTROLLER_CONTROLLERSERVICE_H_
