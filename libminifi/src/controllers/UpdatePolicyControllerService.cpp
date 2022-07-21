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
#include "controllers/UpdatePolicyControllerService.h"

#include <cstdlib>
#include <cstring>

#include <cstdio>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "utils/StringUtils.h"
#if ( defined(__APPLE__) || defined(__MACH__) || defined(BSD))
#include <net/if_dl.h>
#include <net/if_types.h>
#endif
#include "core/state/UpdatePolicy.h"
#include "core/PropertyBuilder.h"
#include "core/PropertyValidation.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::controllers {

const core::Property UpdatePolicyControllerService::AllowAllProperties(
    core::PropertyBuilder::createProperty("Allow All Properties")->withDescription("Allows all properties, which are also not disallowed, to be updated")->withDefaultValue<bool>(
        false)->build());

const core::Property UpdatePolicyControllerService::AllowedProperties(
    core::PropertyBuilder::createProperty("Allowed Properties")->withDescription("Properties for which we will allow updates")->isRequired(false)->build());

const core::Property UpdatePolicyControllerService::DisallowedProperties(
    core::PropertyBuilder::createProperty("Disallowed Properties")->withDescription("Properties for which we will not allow updates")->isRequired(false)->build());

const core::Property UpdatePolicyControllerService::PersistUpdates(
    core::PropertyBuilder::createProperty("Persist Updates")->withDescription("Property that dictates whether updates should persist after a restart")->isRequired(false)->withDefaultValue<bool>(false)
        ->build());

void UpdatePolicyControllerService::initialize() {
  setSupportedProperties(properties());
}

void UpdatePolicyControllerService::yield() {
}

bool UpdatePolicyControllerService::isRunning() {
  return getState() == core::controller::ControllerServiceState::ENABLED;
}

bool UpdatePolicyControllerService::isWorkAvailable() {
  return false;
}

void UpdatePolicyControllerService::onEnable() {
  std::string enableStr;
  std::string persistStr;

  bool enable_all = false;
  if (getProperty(AllowAllProperties.getName(), enableStr)) {
    enable_all = utils::StringUtils::toBool(enableStr).value_or(false);
  }

  if (getProperty(PersistUpdates.getName(), persistStr)) {
    persist_updates_ = utils::StringUtils::toBool(persistStr).value_or(false);
  }

  auto builder = state::UpdatePolicyBuilder::newBuilder(enable_all);

  core::Property all_prop("Allowed Properties", "Properties for which we will allow updates");
  core::Property dall_prop("Disallowed Properties", "Properties for which we will not allow updates");

  if (getProperty(AllowedProperties.getName(), all_prop)) {
    for (const auto &str : all_prop.getValues()) {
      builder->allowPropertyUpdate(str);
    }
  }

  if (getProperty(DisallowedProperties.getName(), dall_prop)) {
    for (const auto &str : dall_prop.getValues()) {
      builder->disallowPropertyUpdate(str);
    }
  }
  policy_ = builder->build();
}

REGISTER_RESOURCE(UpdatePolicyControllerService, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
