/**
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

#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "utils/StringUtils.h"
#include "io/validation.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerFactory.h"
#include "core/state/UpdatePolicy.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::controllers {

/**
 * Purpose: UpdatePolicyControllerService allows a flow specific policy on allowing or disallowing updates.
 * Since the flow dictates the purpose of a device it will also be used to dictate updates to specific components.
 */
class UpdatePolicyControllerService : public core::controller::ControllerService, public std::enable_shared_from_this<UpdatePolicyControllerService> {
 public:
  explicit UpdatePolicyControllerService(std::string name, const utils::Identifier &uuid = {})
      : ControllerService(std::move(name), uuid) {
  }

  explicit UpdatePolicyControllerService(std::string name, const std::shared_ptr<Configure> &configuration)
      : UpdatePolicyControllerService(std::move(name)) {
    setConfiguration(configuration);
    initialize();
  }

  MINIFIAPI static constexpr const char* Description = "UpdatePolicyControllerService allows a flow specific policy on allowing or disallowing updates. "
      "Since the flow dictates the purpose of a device it will also be used to dictate updates to specific components.";

  MINIFIAPI static const core::Property AllowAllProperties;
  MINIFIAPI static const core::Property PersistUpdates;
  MINIFIAPI static const core::Property AllowedProperties;
  MINIFIAPI static const core::Property DisallowedProperties;
  static auto properties() {
    return std::array{
      AllowAllProperties,
      PersistUpdates,
      AllowedProperties,
      DisallowedProperties
    };
  }

  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override;

  void yield() override;

  bool isRunning() override;

  bool isWorkAvailable() override;

  void onEnable() override;

  bool canUpdate(const std::string &property) const {
    return policy_->canUpdate(property);
  }

  bool persistUpdates() const {
    return persist_updates_;
  }

 private:
  bool persist_updates_ = false;
  std::unique_ptr<state::UpdatePolicy> policy_ = std::make_unique<state::UpdatePolicy>(false);
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<UpdatePolicyControllerService>::getLogger();
};

}  // namespace org::apache::nifi::minifi::controllers
