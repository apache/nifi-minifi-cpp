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

#include "minifi-cpp/properties/Configure.h"
#include "core/Core.h"
#include "core/ConfigurableComponent.h"
#include "core/Connectable.h"
#include "minifi-cpp/core/controller/ControllerService.h"

#define ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES \
  bool supportsDynamicProperties() const override { return SupportsDynamicProperties; }

namespace org::apache::nifi::minifi::core::controller {

/**
 * Controller Service base class that contains some pure virtual methods.
 *
 * Design: OnEnable is executed when the controller service is being enabled.
 * Note that keeping state here must be protected  in this function.
 */
class ControllerServiceImpl : public ConfigurableComponentImpl, public ConnectableImpl, public virtual ControllerService {
 public:
  ControllerServiceImpl()
      : ConnectableImpl(core::className<ControllerService>()),
        configuration_(Configure::create()) {
    current_state_ = DISABLED;
  }

  explicit ControllerServiceImpl(std::string_view name, const utils::Identifier &uuid)
      : ConnectableImpl(name, uuid),
        configuration_(Configure::create()) {
    current_state_ = DISABLED;
  }

  explicit ControllerServiceImpl(std::string_view name)
      : ConnectableImpl(name),
        configuration_(Configure::create()) {
    current_state_ = DISABLED;
  }

  void initialize() override {
    current_state_ = ENABLED;
  }

  bool supportsDynamicRelationships() const final {
    return false;
  }

  ~ControllerServiceImpl() override {
    notifyStop();
  }

  /**
   * Replaces the configuration object within the controller service.
   */
  void setConfiguration(const std::shared_ptr<Configure> &configuration) override {
    configuration_ = configuration;
  }

  ControllerServiceState getState() const override {
    return current_state_.load();
  }

  /**
   * Function is called when Controller Services are enabled and being run
   */
  void onEnable() override {
  }

  /**
   * Function is called when Controller Services are disabled
   */
  void notifyStop() override {
  }

  void setState(ControllerServiceState state) override {
    current_state_ = state;
    if (state == DISABLED) {
      notifyStop();
    }
  }

  void setLinkedControllerServices(const std::vector<std::shared_ptr<controller::ControllerService>> &services) override {
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
