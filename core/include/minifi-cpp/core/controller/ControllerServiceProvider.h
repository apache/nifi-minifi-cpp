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
#include <string_view>
#include <utility>
#include <future>
#include <vector>

#include "minifi-cpp/core/Core.h"
#include "ControllerServiceLookup.h"
#include "minifi-cpp/core/ConfigurableComponent.h"
#include "ControllerServiceNode.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceProvider : public virtual CoreComponent, public virtual ConfigurableComponent, public virtual ControllerServiceLookup, public utils::EnableSharedFromThis {
 public:
  ~ControllerServiceProvider() override = default;

  virtual std::shared_ptr<ControllerServiceNode> createControllerService(const std::string &type, const std::string &longType, const std::string &id, bool firstTimeAdded) = 0;
  virtual ControllerServiceNode* getControllerServiceNode(const std::string &id) const = 0;
  virtual void clearControllerServices() = 0;
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> getAllControllerServices() = 0;
  virtual void putControllerServiceNode(const std::string& identifier, const std::shared_ptr<ControllerServiceNode>& controller_service_node) = 0;
  virtual void enableAllControllerServices() = 0;
  virtual void disableAllControllerServices() = 0;
};

}  // namespace org::apache::nifi::minifi::core::controller
