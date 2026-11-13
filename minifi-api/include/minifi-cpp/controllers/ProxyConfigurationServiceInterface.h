// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <optional>
#include <cstdint>

#include "minifi-cpp/core/controller/ControllerServiceHandle.h"
#include "minifi-cpp/core/ControllerServiceTypeDefinition.h"
#include "minifi-cpp/agent/agent_version.h"

namespace org::apache::nifi::minifi::controllers {

enum class ProxyType {
  DIRECT,
  HTTP
};

struct BasicAuthCredentials {
  std::string username;
  std::string password;
};

class ProxyConfigurationServiceInterface : public virtual core::controller::ControllerServiceHandle {
 public:
  static constexpr auto ProvidesApi = core::ControllerServiceTypeDefinition {
    .artifact = "minifi-system",
    .group = "org.apache.nifi.minifi",
    .type = "org.apache.nifi.minifi.controllers.ProxyConfigurationServiceInterface",
    .version = MINIFI_VERSION_STR
  };

  virtual std::string getHost() const = 0;
  virtual uint16_t getPort() const = 0;
  virtual std::optional<BasicAuthCredentials> getProxyCredentials() const = 0;
  virtual ProxyType getProxyType() const = 0;
};

}  // namespace org::apache::nifi::minifi::controllers
