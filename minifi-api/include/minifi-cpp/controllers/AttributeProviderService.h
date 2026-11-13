// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <optional>
#include <unordered_map>
#include <vector>

#include "minifi-cpp/core/controller/ControllerServiceHandle.h"
#include "minifi-cpp/core/ControllerServiceTypeDefinition.h"
#include "minifi-cpp/agent/agent_version.h"

namespace org::apache::nifi::minifi::controllers {

class AttributeProviderService : public core::controller::ControllerServiceHandle {
 public:
  static constexpr auto ProvidesApi = core::ControllerServiceTypeDefinition {
    .artifact = "minifi-system",
    .group = "org.apache.nifi.minifi",
    .type = "org.apache.nifi.minifi.controllers.AttributeProviderService",
    .version = MINIFI_VERSION_STR
  };

  using AttributeMap = std::unordered_map<std::string, std::string>;
  virtual std::optional<std::vector<AttributeMap>> getAttributes() = 0;
  [[nodiscard]] virtual std::string_view name() const = 0;
};

}  // namespace org::apache::nifi::minifi::controllers
