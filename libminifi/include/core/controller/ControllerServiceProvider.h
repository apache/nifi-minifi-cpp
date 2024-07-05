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
#include "core/ConfigurableComponent.h"
#include "ControllerServiceNode.h"
#include "ControllerServiceNodeMap.h"
#include "core/ClassLoader.h"
#include "utils/Monitors.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceProvider : public CoreComponent, public ConfigurableComponent, public ControllerServiceLookup {
 public:
  explicit ControllerServiceProvider(std::string_view name)
      : CoreComponent(name),
        controller_map_{std::make_unique<ControllerServiceNodeMap>()} {
  }

  explicit ControllerServiceProvider(std::unique_ptr<ControllerServiceNodeMap> services)
      : CoreComponent(core::className<ControllerServiceProvider>()),
        controller_map_(std::move(services)) {
  }

  explicit ControllerServiceProvider(std::string_view name, std::unique_ptr<ControllerServiceNodeMap> services)
      : CoreComponent(name),
        controller_map_(std::move(services)) {
  }

  ControllerServiceProvider(const ControllerServiceProvider &other) = delete;
  ControllerServiceProvider(ControllerServiceProvider &&other) = delete;

  ControllerServiceProvider& operator=(const ControllerServiceProvider &other) = delete;
  ControllerServiceProvider& operator=(ControllerServiceProvider &&other) = delete;

  ~ControllerServiceProvider() override = default;

  virtual std::shared_ptr<ControllerServiceNode> createControllerService(const std::string &type, const std::string &longType, const std::string &id, bool firstTimeAdded) = 0;

  virtual ControllerServiceNode* getControllerServiceNode(const std::string &id) const {
    return controller_map_->get(id);
  }

  virtual ControllerServiceNode* getControllerServiceNode(const std::string &id, const utils::Identifier &processor_or_controller_uuid) const {
    return controller_map_->get(id, processor_or_controller_uuid);
  }

  virtual void clearControllerServices() = 0;

  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> getAllControllerServices() {
    return controller_map_->getAllControllerServices();
  }

  std::shared_ptr<ControllerService> getControllerService(const std::string &identifier) const override;
  std::shared_ptr<ControllerService> getControllerService(const std::string &identifier, const utils::Identifier &processor_uuid) const override;

  void putControllerServiceNode(const std::string& identifier, const std::shared_ptr<ControllerServiceNode>& controller_service_node, ProcessGroup* process_group);

  bool isControllerServiceEnabled(const std::string &identifier) override {
    const ControllerServiceNode* const node = getControllerServiceNode(identifier);
    if (nullptr != node) {
      return linkedServicesAre(ENABLED, node);
    } else {
      return false;
    }
  }

  bool isControllerServiceEnabling(const std::string &identifier) override {
    const ControllerServiceNode* const node = getControllerServiceNode(identifier);
    if (nullptr != node) {
      return linkedServicesAre(ENABLING, node);
    } else {
      return false;
    }
  }

  const std::string getControllerServiceName(const std::string &identifier) const override {
    std::shared_ptr<ControllerService> node = getControllerService(identifier);
    if (nullptr != node) {
      return node->getName();
    } else {
      return "";
    }
  }

  virtual void enableAllControllerServices() = 0;

  virtual void disableAllControllerServices() = 0;

  bool supportsDynamicProperties() const final {
    return false;
  }

  bool supportsDynamicRelationships() const final {
    return false;
  }

 protected:
  inline bool linkedServicesAre(ControllerServiceState state, const ControllerServiceNode* node) {
    if (node->getControllerServiceImplementation()->getState() == state) {
      for (auto child_service : node->getLinkedControllerServices()) {
        if (child_service->getControllerServiceImplementation()->getState() != state) {
          return false;
        }
      }
      return true;
    } else {
      return false;
    }
  }

  bool canEdit() override {
    return true;
  }

  std::vector<core::controller::ControllerServiceNode*> findLinkedComponents(core::controller::ControllerServiceNode* referenceNode) {
    std::vector<core::controller::ControllerServiceNode*> references;

    for (auto* linked_node : referenceNode->getLinkedControllerServices()) {
      references.push_back(linked_node);
      std::vector<core::controller::ControllerServiceNode*> linked_references = findLinkedComponents(linked_node);

      auto removal_predicate = [&linked_references](core::controller::ControllerServiceNode* key) ->bool {
        return std::find(linked_references.begin(), linked_references.end(), key) != linked_references.end();
      };

      references.erase(std::remove_if(references.begin(), references.end(), removal_predicate), references.end());

      references.insert(std::end(references), linked_references.begin(), linked_references.end());
    }
    return references;
  }

  std::unique_ptr<ControllerServiceNodeMap> controller_map_;
};

}  // namespace org::apache::nifi::minifi::core::controller
