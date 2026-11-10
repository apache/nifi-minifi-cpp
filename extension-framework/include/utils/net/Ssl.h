// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <memory>
#include <optional>

#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::utils::net {

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

  SslServerOptions(SslData cert_data, ClientAuthOption client_auth_option)
      : cert_data(cert_data),
      client_auth_option(client_auth_option) {}
};

std::optional<utils::net::SslData> getSslData(const core::ProcessContext& context, const core::PropertyReference& ssl_prop, const std::shared_ptr<core::logging::Logger>& logger);

}  // namespace org::apache::nifi::minifi::utils::net
