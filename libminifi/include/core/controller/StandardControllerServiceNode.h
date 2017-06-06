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
#ifndef LIBMINIFI_INCLUDE_CORE_CONTROLLER_STANDARDCONTROLLERSERVICENODE_H_
#define LIBMINIFI_INCLUDE_CORE_CONTROLLER_STANDARDCONTROLLERSERVICENODE_H_

#include "core/Core.h"
#include "ControllerServiceNode.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ProcessGroup.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace controller {

class StandardControllerServiceNode : public ControllerServiceNode {
 public:

  explicit StandardControllerServiceNode(std::shared_ptr<ControllerService> service, std::shared_ptr<ControllerServiceProvider> provider, const std::string &id,
                                         std::shared_ptr<Configure> configuration)
      : ControllerServiceNode(service, id, configuration),
        provider(provider),
        logger_(logging::LoggerFactory<StandardControllerServiceNode>::getLogger()) {
  }

  explicit StandardControllerServiceNode(std::shared_ptr<ControllerService> service, const std::string &id, std::shared_ptr<Configure> configuration)
      : ControllerServiceNode(service, id, configuration),
        provider(nullptr),
        logger_(logging::LoggerFactory<StandardControllerServiceNode>::getLogger()) {
  }

  std::shared_ptr<core::ProcessGroup> &getProcessGroup();

  void setProcessGroup(std::shared_ptr<ProcessGroup> &processGroup);

  StandardControllerServiceNode(const StandardControllerServiceNode &other) = delete;
  StandardControllerServiceNode &operator=(const StandardControllerServiceNode &parent) = delete;

  /**
   * Initializes the controller service node.
   */
  virtual void initialize() {
    ControllerServiceNode::initialize();
    active = false;
  }

  bool canEnable() {
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

  bool enable();

  bool disable() {
    controller_service_->setState(DISABLED);
    active = false;
    return true;
  }

 protected:

  // controller service provider.
  std::shared_ptr<ControllerServiceProvider> provider;

  // process group.
  std::shared_ptr<core::ProcessGroup> process_group_;

  std::mutex mutex_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace controller */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_CONTROLLER_STANDARDCONTROLLERSERVICENODE_H_ */
