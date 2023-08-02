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
#pragma once

#include <string>
#include <memory>
#include <optional>

#include "core/ProcessContext.h"
#include "core/Property.h"
#include "core/logging/Logger.h"
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
