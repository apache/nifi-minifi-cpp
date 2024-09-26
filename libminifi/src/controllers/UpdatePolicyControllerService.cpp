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
#include "core/Resource.h"

namespace org::apache::nifi::minifi::controllers {

void UpdatePolicyControllerService::initialize() {
  setSupportedProperties(Properties);
}

void UpdatePolicyControllerService::yield() {
}

bool UpdatePolicyControllerService::isRunning() const {
  return getState() == core::controller::ControllerServiceState::ENABLED;
}

bool UpdatePolicyControllerService::isWorkAvailable() {
  return false;
}

void UpdatePolicyControllerService::onEnable() {
  std::string enableStr;
  std::string persistStr;

  bool enable_all = false;
  if (getProperty(AllowAllProperties, enableStr)) {
    enable_all = utils::string::toBool(enableStr).value_or(false);
  }

  if (getProperty(PersistUpdates, persistStr)) {
    persist_updates_ = utils::string::toBool(persistStr).value_or(false);
  }

  auto builder = state::UpdatePolicyBuilder::newBuilder(enable_all);

  core::Property all_prop("Allowed Properties", "Properties for which we will allow updates");
  core::Property dall_prop("Disallowed Properties", "Properties for which we will not allow updates");

  if (getProperty(AllowedProperties, all_prop)) {
    for (const auto &str : all_prop.getValues()) {
      builder->allowPropertyUpdate(str);
    }
  }

  if (getProperty(DisallowedProperties, dall_prop)) {
    for (const auto &str : dall_prop.getValues()) {
      builder->disallowPropertyUpdate(str);
    }
  }
  policy_ = builder->build();
}

REGISTER_RESOURCE(UpdatePolicyControllerService, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
