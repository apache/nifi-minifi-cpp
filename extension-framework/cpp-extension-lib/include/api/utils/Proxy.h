// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace org::apache::nifi::minifi::api::utils {

enum class ProxyType {
  DIRECT,
  HTTP
};

struct BasicAuthCredentials {
  std::string username;
  std::string password;
};

struct ProxyData {
  ProxyType proxy_type;
  std::string host;
  uint16_t port;
  std::optional<BasicAuthCredentials> proxy_credentials;
};

}  // namespace org::apache::nifi::minifi::api::utils
