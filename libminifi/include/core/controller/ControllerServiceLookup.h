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
#include <map>
#include "minifi-cpp/core/Core.h"
#include "minifi-cpp/core/ConfigurableComponent.h"
#include "core/controller/ControllerService.h"

namespace org::apache::nifi::minifi::core::controller {

/**
 * Controller Service Lookup pure virtual class.
 *
 * Purpose: Provide a mechanism that controllers can lookup information about
 * controller services.
 *
 */
class ControllerServiceLookup {
 public:
  ControllerServiceLookup() = default;

  virtual ~ControllerServiceLookup() = default;

  /**
   * Gets the controller service via the provided identifier. This overload returns the controller service in a global scope from all
   * available controller services in the flow.
   * @param identifier reference string for controller service.
   * @return controller service reference.
   */
  virtual std::shared_ptr<ControllerService> getControllerService(const std::string &identifier) const = 0;

  /**
   * Gets the controller service in the scope of the processor via the provided identifier.
   * @param identifier reference string for controller service.
   * @param processor_uuid uuid of the processor
   * @return controller service reference.
   */
  virtual std::shared_ptr<ControllerService> getControllerService(const std::string &identifier, const utils::Identifier &processor_uuid) const = 0;
};

}  // namespace org::apache::nifi::minifi::core::controller
