/**
 *
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

#include "SmbConnectionControllerService.h"
#include "core/Resource.h"
#include "utils/OsUtils.h"
#include "utils/expected.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::extensions::smb {

void SmbConnectionControllerService::initialize() {
  setSupportedProperties(Properties);
}

void SmbConnectionControllerService::onEnable()  {
  std::string hostname = getProperty(Hostname.name) | utils::orThrow("Required property");
  std::string share = getProperty(Share.name) | utils::orThrow("Required property");

  server_path_ = "\\\\" + hostname + "\\" + share;

  auto password = getProperty(Password.name);
  auto username = getProperty(Username.name);

  if (password.has_value() != username.has_value())
    throw Exception(PROCESS_SCHEDULE_EXCEPTION,  "Either both a username and a password, or neither of them should be provided.");

  if (username.has_value())
    credentials_.emplace(Credentials{.username = *username, .password = *password});
  else
    credentials_.reset();

  net_resource_ = {
      .dwType = RESOURCETYPE_DISK,
      .lpLocalName = nullptr,
      .lpRemoteName = server_path_.data(),
      .lpProvider = nullptr,
  };
}

void SmbConnectionControllerService::notifyStop() {
  auto disconnection_result = disconnect();
  if (!disconnection_result)
    logger_->log_error("Error while disconnecting from SMB: {}", disconnection_result.error().message());
}

nonstd::expected<void, std::error_code> SmbConnectionControllerService::connect() {
  auto connection_result = WNetAddConnection2A(&net_resource_,
      credentials_ ? credentials_->password.c_str() : nullptr,
      credentials_ ? credentials_->username.c_str() : nullptr,
      CONNECT_TEMPORARY);
  if (connection_result == NO_ERROR)
    return {};

  return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(connection_result));
}

nonstd::expected<void, std::error_code> SmbConnectionControllerService::disconnect() {
  auto disconnection_result = WNetCancelConnection2A(server_path_.c_str(), 0, true);
  if (disconnection_result == NO_ERROR)
    return {};

  return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(disconnection_result));
}

bool SmbConnectionControllerService::isConnected() {
  std::error_code error_code;
  auto exists = std::filesystem::exists(server_path_, error_code);
  if (error_code) {
    logger_->log_debug("std::filesystem::exists({}) failed due to {}", server_path_, error_code.message());
    return false;
  }
  return exists;
}

std::error_code SmbConnectionControllerService::validateConnection() {
  if (isConnected()) {
    return std::error_code();
  }
  auto connection_result = connect();
  if (!connection_result) {
    return connection_result.error();
  }

  return std::error_code();
}

REGISTER_RESOURCE(SmbConnectionControllerService, ControllerService);

}  // namespace org::apache::nifi::minifi::extensions::smb
