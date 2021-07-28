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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_UPDATEPOLICYCONTROLLERSERVICE_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_UPDATEPOLICYCONTROLLERSERVICE_H_

#include <string>
#include <iostream>
#include <memory>
#include <limits>
#include "utils/StringUtils.h"
#include "io/validation.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/state/UpdatePolicy.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

/**
 * Purpose: UpdatePolicyControllerService allows a flow specific policy on allowing or disallowing updates.
 * Since the flow dictates the purpose of a device it will also be used to dictate updates to specific components.
 */
class UpdatePolicyControllerService : public core::controller::ControllerService, public std::enable_shared_from_this<UpdatePolicyControllerService> {
 public:
  explicit UpdatePolicyControllerService(const std::string &name, const utils::Identifier &uuid = {})
      : ControllerService(name, uuid),
        persist_updates_(false),
        policy_(new state::UpdatePolicy(false)),
        logger_(logging::LoggerFactory<UpdatePolicyControllerService>::getLogger()) {
  }

  explicit UpdatePolicyControllerService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : UpdatePolicyControllerService(name) {
    setConfiguration(configuration);
    initialize();
  }

  MINIFIAPI static core::Property AllowAllProperties;
  MINIFIAPI static core::Property PersistUpdates;
  MINIFIAPI static core::Property AllowedProperties;
  MINIFIAPI static core::Property DisallowedProperties;

  void initialize();

  void yield();

  bool isRunning();

  bool isWorkAvailable();

  void onEnable();

  bool canUpdate(const std::string &property) const {
    return policy_->canUpdate(property);
  }

  bool persistUpdates() const {
    return persist_updates_;
  }

 private:
  bool persist_updates_;
  std::unique_ptr<state::UpdatePolicy> policy_;
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CONTROLLERS_UPDATEPOLICYCONTROLLERSERVICE_H_
