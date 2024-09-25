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
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/state/UpdatePolicy.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::controllers {

/**
 * Purpose: UpdatePolicyControllerService allows a flow specific policy on allowing or disallowing updates.
 * Since the flow dictates the purpose of a device it will also be used to dictate updates to specific components.
 */
class UpdatePolicyControllerService : public core::controller::ControllerServiceImpl, public std::enable_shared_from_this<UpdatePolicyControllerService> {
 public:
  explicit UpdatePolicyControllerService(std::string_view name, const utils::Identifier &uuid = {})
      : ControllerServiceImpl(name, uuid) {
  }

  explicit UpdatePolicyControllerService(std::string_view name, const std::shared_ptr<Configure> &configuration)
      : UpdatePolicyControllerService(name) {
    setConfiguration(configuration);
    initialize();
  }

  MINIFIAPI static constexpr const char* Description = "UpdatePolicyControllerService allows a flow specific policy on allowing or disallowing updates. "
                                                       "Since the flow dictates the purpose of a device it will also be used to dictate updates to specific components.";

  MINIFIAPI static constexpr auto AllowAllProperties = core::PropertyDefinitionBuilder<>::createProperty("Allow All Properties")
      .withDescription("Allows all properties, which are also not disallowed, to be updated")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  MINIFIAPI static constexpr auto PersistUpdates = core::PropertyDefinitionBuilder<>::createProperty("Persist Updates")
      .withDescription("Property that dictates whether updates should persist after a restart")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  MINIFIAPI static constexpr auto AllowedProperties = core::PropertyDefinitionBuilder<>::createProperty("Allowed Properties")
      .withDescription("Properties for which we will allow updates")
      .isRequired(false)
      .build();
  MINIFIAPI static constexpr auto DisallowedProperties = core::PropertyDefinitionBuilder<>::createProperty("Disallowed Properties")
      .withDescription("Properties for which we will not allow updates")
      .isRequired(false)
      .build();
  MINIFIAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
                                                                                          AllowAllProperties,
                                                                                          PersistUpdates,
                                                                                          AllowedProperties,
                                                                                          DisallowedProperties
                                                                                      });

  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override;

  void yield() override;

  bool isRunning() const override;

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
