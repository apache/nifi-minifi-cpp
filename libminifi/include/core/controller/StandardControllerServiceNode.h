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

#include "core/Core.h"
#include "ControllerServiceNode.h"
#include "core/logging/LoggerFactory.h"
#include "core/ProcessGroup.h"

namespace org::apache::nifi::minifi::core::controller {

class StandardControllerServiceNode : public ControllerServiceNodeImpl {
 public:
  explicit StandardControllerServiceNode(std::shared_ptr<ControllerService> service, std::shared_ptr<ControllerServiceProvider> provider, std::string id,
                                         std::shared_ptr<Configure> configuration)
      : ControllerServiceNodeImpl(std::move(service), std::move(id), std::move(configuration)),
        provider(std::move(provider)),
        logger_(logging::LoggerFactory<StandardControllerServiceNode>::getLogger()) {
  }

  explicit StandardControllerServiceNode(std::shared_ptr<ControllerService> service, std::string id, std::shared_ptr<Configure> configuration)
      : ControllerServiceNodeImpl(std::move(service), std::move(id), std::move(configuration)),
        provider(nullptr),
        logger_(logging::LoggerFactory<StandardControllerServiceNode>::getLogger()) {
  }

  StandardControllerServiceNode(const StandardControllerServiceNode &other) = delete;
  StandardControllerServiceNode &operator=(const StandardControllerServiceNode &parent) = delete;

  /**
   * Initializes the controller service node.
   */
  void initialize() override {
    ControllerServiceNodeImpl::initialize();
    active = false;
  }

  bool canEnable() override {
    if (!active.load()) {
      for (auto linked_service : linked_controller_services_) {
        if (!linked_service->canEnable()) {
          return false;
        }
      }
      return true;
    } else {
      return false;
    }
  }

  bool enable() override;

  bool disable() override {
    controller_service_->setState(DISABLED);
    active = false;
    return true;
  }

 protected:
  // controller service provider.
  std::shared_ptr<ControllerServiceProvider> provider;

  std::mutex mutex_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::controller
