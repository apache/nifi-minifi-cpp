// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <filesystem>

namespace org::apache::nifi::minifi::api::utils::net {

enum class ClientAuthOption {
  NONE,
  WANT,
  REQUIRED
};

struct SslData {
  std::filesystem::path ca_loc;
  std::filesystem::path cert_loc;
  std::filesystem::path key_loc;
  std::string key_pw;

  bool isValid() const {
    return !cert_loc.empty() && !key_loc.empty();
  }
};

struct SslServerOptions {
  SslData cert_data;
  ClientAuthOption client_auth_option;
};

}  // namespace org::apache::nifi::minifi::api::utils::net
