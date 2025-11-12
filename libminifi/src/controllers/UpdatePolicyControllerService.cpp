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

void UpdatePolicyControllerService::onEnable() {
  const bool enable_all = (getProperty(AllowAllProperties.name) | utils::andThen(parsing::parseBool)).value_or(false);
  persist_updates_ = (getProperty(PersistUpdates.name) | utils::andThen(parsing::parseBool)).value_or(false);

  const auto builder = state::UpdatePolicyBuilder::newBuilder(enable_all);

  if (auto allowed_props = getAllPropertyValues(AllowedProperties.name)) {
    for (const auto& allowed_prop : *allowed_props) {
      builder->allowPropertyUpdate(allowed_prop);
    }
  }

  if (auto disallowed_props = getAllPropertyValues(DisallowedProperties.name)) {
    for (const auto& disallowed_prop : *disallowed_props) {
      builder->disallowPropertyUpdate(disallowed_prop);
    }
  }
  policy_ = builder->build();
}

REGISTER_RESOURCE(UpdatePolicyControllerService, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
