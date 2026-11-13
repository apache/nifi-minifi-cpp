// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
