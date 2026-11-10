// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <optional>

#include "minifi-cpp/controllers/ProxyConfigurationServiceInterface.h"

namespace org::apache::nifi::minifi::controllers {

struct ProxyConfiguration {
  ProxyType proxy_type = ProxyType::DIRECT;
  std::string proxy_host;
  uint16_t proxy_port = 0;
  std::optional<BasicAuthCredentials> proxy_credentials;
};

}  // namespace org::apache::nifi::minifi::controllers
