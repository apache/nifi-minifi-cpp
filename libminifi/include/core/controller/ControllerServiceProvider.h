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

#include "core/Core.h"
#include "ControllerServiceLookup.h"
#include "core/ConfigurableComponentImpl.h"
#include "ControllerServiceNode.h"
#include "ControllerServiceNodeMap.h"
#include "core/ClassLoader.h"
#include "utils/Monitors.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceProvider : public CoreComponentImpl, public ConfigurableComponentImpl, public virtual ControllerServiceLookup, public utils::EnableSharedFromThis {
 public:
  explicit ControllerServiceProvider(std::string_view name)
      : CoreComponentImpl(name),
        controller_map_{std::make_unique<ControllerServiceNodeMap>()} {
  }

  explicit ControllerServiceProvider(std::unique_ptr<ControllerServiceNodeMap> services)
      : CoreComponentImpl(core::className<ControllerServiceProvider>()),
        controller_map_(std::move(services)) {
  }

  explicit ControllerServiceProvider(std::string_view name, std::unique_ptr<ControllerServiceNodeMap> services)
      : CoreComponentImpl(name),
        controller_map_(std::move(services)) {
  }

  ControllerServiceProvider(const ControllerServiceProvider &other) = delete;
  ControllerServiceProvider(ControllerServiceProvider &&other) = delete;

  ControllerServiceProvider& operator=(const ControllerServiceProvider &other) = delete;
  ControllerServiceProvider& operator=(ControllerServiceProvider &&other) = delete;

  ~ControllerServiceProvider() override = default;

  virtual ControllerServiceNode* getControllerServiceNode(const std::string &id) const {
    return controller_map_->get(id);
  }

  virtual ControllerServiceNode* getControllerServiceNode(const std::string &id, const utils::Identifier &processor_or_controller_uuid) const {
    return controller_map_->get(id, processor_or_controller_uuid);
  }

  virtual std::shared_ptr<ControllerServiceNode> createControllerService(const std::string &type, const std::string &id) = 0;

  virtual void clearControllerServices() = 0;

  virtual void enableAllControllerServices() = 0;

  virtual void disableAllControllerServices() = 0;

  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> getAllControllerServices() {
    return controller_map_->getAllControllerServices();
  }

  std::shared_ptr<ControllerService> getControllerService(const std::string &identifier) const override;
  std::shared_ptr<ControllerService> getControllerService(const std::string &identifier, const utils::Identifier &processor_uuid) const override;

  virtual void putControllerServiceNode(const std::string& identifier, const std::shared_ptr<ControllerServiceNode>& controller_service_node, ProcessGroup* process_group);

  bool supportsDynamicProperties() const final {
    return false;
  }

  bool supportsDynamicRelationships() const final {
    return false;
  }

 protected:
  bool canEdit() override {
    return true;
  }

  std::unique_ptr<ControllerServiceNodeMap> controller_map_;
};

}  // namespace org::apache::nifi::minifi::core::controller
