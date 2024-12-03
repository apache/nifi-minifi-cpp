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
#include "minifi-cpp/core/Core.h"
#include "minifi-cpp/core/ConfigurableComponent.h"
#include "minifi-cpp/core/Connectable.h"

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
class ControllerService : public virtual ConfigurableComponent, public virtual Connectable {
 public:
  virtual void setConfiguration(const std::shared_ptr<Configure> &configuration) = 0;
  virtual ControllerServiceState getState() const = 0;
  virtual void onEnable() = 0;
  virtual void notifyStop() = 0;
  virtual void setState(ControllerServiceState state) = 0;
  virtual void setLinkedControllerServices(const std::vector<std::shared_ptr<controller::ControllerService>> &services) = 0;
};

}  // namespace org::apache::nifi::minifi::core::controller
