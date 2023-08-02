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
#include "ControllerServiceMap.h"
#include "core/ClassLoader.h"
#include "utils/Monitors.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceProvider : public CoreComponent, public ConfigurableComponent, public ControllerServiceLookup {
 public:
  explicit ControllerServiceProvider(std::string_view name)
      : CoreComponent(name) {
    controller_map_ = std::make_shared<ControllerServiceMap>();
  }

  explicit ControllerServiceProvider(std::shared_ptr<ControllerServiceMap> services)
      : CoreComponent(core::className<ControllerServiceProvider>()),
        controller_map_(std::move(services)) {
  }

  explicit ControllerServiceProvider(std::string_view name, std::shared_ptr<ControllerServiceMap> services)
      : CoreComponent(name),
        controller_map_(std::move(services)) {
  }

  ControllerServiceProvider(const ControllerServiceProvider &other) = delete;
  ControllerServiceProvider(ControllerServiceProvider &&other) = delete;

  ControllerServiceProvider& operator=(const ControllerServiceProvider &other) = delete;
  ControllerServiceProvider& operator=(ControllerServiceProvider &&other) = delete;

  virtual ~ControllerServiceProvider() = default;

  /**
   * Creates a controller service node wrapping the controller service
   *
   * @param type service type.
   * @param id controller service identifier.
   * @return shared pointer to the controller service node.
   */
  virtual std::shared_ptr<ControllerServiceNode> createControllerService(const std::string &type, const std::string &longType, const std::string &id, bool firstTimeAdded) = 0;

  /**
   * Gets a controller service node wrapping the controller service
   *
   * @param type service type.
   * @param id controller service identifier.
   * @return shared pointer to the controller service node.
   */
  virtual std::shared_ptr<ControllerServiceNode> getControllerServiceNode(const std::string &id) const {
    return controller_map_->getControllerServiceNode(id);
  }

  /**
   * Removes all controller services.
   */
  virtual void clearControllerServices() = 0;

  /**
   * Gets a list of all controller services.
   */
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> getAllControllerServices() {
    return controller_map_->getAllControllerServices();
  }

  /**
   * Gets the controller service for the provided identifier
   * @param identifier service identifier.
   */
  std::shared_ptr<ControllerService> getControllerService(const std::string &identifier) const override;

  /**
   * Determines if Controller service is enabled.
   * @param identifier service identifier.
   */
  bool isControllerServiceEnabled(const std::string &identifier) override {
    std::shared_ptr<ControllerServiceNode> node = getControllerServiceNode(identifier);
    if (nullptr != node) {
      return linkedServicesAre(ENABLED, node);
    } else {
      return false;
    }
  }

  /**
   * Determines if Controller service is being enabled.
   * @param identifier service identifier.
   */
  bool isControllerServiceEnabling(const std::string &identifier) override {
    std::shared_ptr<ControllerServiceNode> node = getControllerServiceNode(identifier);
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
  /**
   * verifies that linked services match the provided state.
   */
  inline bool linkedServicesAre(ControllerServiceState state, const std::shared_ptr<ControllerServiceNode> &node) {
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

  /**
   * Finds linked components
   * @param referenceNode reference node from whcih we will find linked references.
   */
  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> findLinkedComponents(std::shared_ptr<core::controller::ControllerServiceNode> &referenceNode) {
    std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> references;

    for (std::shared_ptr<core::controller::ControllerServiceNode> linked_node : referenceNode->getLinkedControllerServices()) {
      references.push_back(linked_node);
      std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> linked_references = findLinkedComponents(linked_node);

      auto removal_predicate = [&linked_references](std::shared_ptr<core::controller::ControllerServiceNode> key) ->bool {
        return std::find(linked_references.begin(), linked_references.end(), key) != linked_references.end();
      };

      references.erase(std::remove_if(references.begin(), references.end(), removal_predicate), references.end());

      references.insert(std::end(references), linked_references.begin(), linked_references.end());
    }
    return references;
  }

  std::shared_ptr<ControllerServiceMap> controller_map_;
};

}  // namespace org::apache::nifi::minifi::core::controller
