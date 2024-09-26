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
#include <filesystem>
#include <string>
#include <memory>

#include "ProcessContext.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/controller/ControllerService.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Enum.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::extensions::smb {

class SmbConnectionControllerService : public core::controller::ControllerServiceImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "SMB Connection Controller Service";

  EXTENSIONAPI static constexpr auto Hostname  = core::PropertyDefinitionBuilder<>::createProperty("Hostname")
      .withDescription("The network host to which files should be written.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Share = core::PropertyDefinitionBuilder<>::createProperty("Share")
      .withDescription(R"(The network share to which files should be written. This is the "first folder" after the hostname: \\hostname\[share]\dir1\dir2)")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Domain  = core::PropertyDefinitionBuilder<>::createProperty("Domain")
      .withDescription("The domain used for authentication. Optional, in most cases username and password is sufficient.")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto Username  = core::PropertyDefinitionBuilder<>::createProperty("Username")
      .withDescription("The username used for authentication. If no username is set then anonymous authentication is attempted.")
      .isRequired(false)
      .withDependentProperties({"Password"})
      .build();
  EXTENSIONAPI static constexpr auto Password  = core::PropertyDefinitionBuilder<>::createProperty("Password")
      .withDescription("The password used for authentication. Required if Username is set.")
      .isRequired(false)
      .withDependentProperties({"Username"})
      .isSensitive(true)
      .build();

  static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Hostname,
      Share,
      Domain,
      Username,
      Password
  });

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  using ControllerServiceImpl::ControllerServiceImpl;

  void initialize() override;

  void onEnable() override;
  void notifyStop() override;

  void yield() override {}
  bool isRunning() const override { return getState() == core::controller::ControllerServiceState::ENABLED; }
  bool isWorkAvailable() override { return false; }

  virtual std::error_code validateConnection();
  virtual std::filesystem::path getPath() const { return server_path_; }

  static gsl::not_null<std::shared_ptr<SmbConnectionControllerService>> getFromProperty(const core::ProcessContext& context, const core::PropertyReference& property);

 private:
  nonstd::expected<void, std::error_code> connect();
  nonstd::expected<void, std::error_code> disconnect();
  bool isConnected();

  struct Credentials {
    std::string username;
    std::string password;
  };

  std::optional<Credentials> credentials_;
  std::string server_path_;
  NETRESOURCEA net_resource_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<SmbConnectionControllerService>::getLogger(uuid_);
};
}  // namespace org::apache::nifi::minifi::extensions::smb
