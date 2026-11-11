// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

#include "core/Core.h"
#include "core/ConfigurationFactory.h"
#include "core/FlowConfiguration.h"

#include "core/yaml/YamlConfiguration.h"
#include "core/flow/AdaptiveConfiguration.h"

namespace org::apache::nifi::minifi::core {

std::unique_ptr<core::FlowConfiguration> createFlowConfiguration(const ConfigurationContext& ctx, const std::optional<std::string>& configuration_class_name, bool fail_safe) {
  std::string class_name_lc;
  if (configuration_class_name) {
    class_name_lc = configuration_class_name.value();
  } else if (ctx.path) {
    class_name_lc = "adaptiveconfiguration";
  } else {
    throw std::runtime_error("Neither configuration class nor config file path has been specified");
  }

  std::transform(class_name_lc.begin(), class_name_lc.end(), class_name_lc.begin(), ::tolower);
  try {
    if (class_name_lc == "flowconfiguration") {
      // load the base configuration.
      return std::make_unique<core::FlowConfiguration>(ctx);
    } else if (class_name_lc == "yamlconfiguration") {
      // only load if the class is defined.
      return std::make_unique<core::YamlConfiguration>(ctx);
    } else if (class_name_lc == "adaptiveconfiguration") {
      return std::make_unique<core::flow::AdaptiveConfiguration>(ctx);
    } else {
      if (fail_safe) {
        return std::make_unique<core::FlowConfiguration>(ctx);
      } else {
        throw std::runtime_error("Support for the provided configuration class could not be found");
      }
    }
  } catch (const std::runtime_error &) {
    if (fail_safe) {
      return std::make_unique<core::FlowConfiguration>(ctx);
    }
  }

  throw std::runtime_error("Support for the provided configuration class could not be found");
}

}  // namespace org::apache::nifi::minifi::core
