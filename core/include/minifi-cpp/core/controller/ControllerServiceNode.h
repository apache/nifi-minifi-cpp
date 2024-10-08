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

#include "minifi-cpp/core/Core.h"
#include "minifi-cpp/core/ConfigurableComponent.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/properties/Configure.h"
#include "ControllerService.h"
#include "io/validation.h"
#include "Exception.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceNode : public virtual CoreComponent, public virtual ConfigurableComponent {
 public:
  virtual std::shared_ptr<ControllerService> getControllerServiceImplementation() = 0;
  virtual const ControllerService* getControllerServiceImplementation() const = 0;
  virtual const std::vector<ControllerServiceNode*>& getLinkedControllerServices() const = 0;
  virtual bool canEnable() = 0;
  virtual bool enabled() = 0;
  virtual bool enable() = 0;
  virtual bool disable() = 0;
};

}  // namespace org::apache::nifi::minifi::core::controller
