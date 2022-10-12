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

#pragma once

#include <string>
#include <memory>
#include <utility>

#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"
#include "storage/AzureStorageCredentials.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::azure::controllers {

class AzureStorageCredentialsService : public core::controller::ControllerService {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Azure Storage Credentials Management Service";

  EXTENSIONAPI static const core::Property StorageAccountName;
  EXTENSIONAPI static const core::Property StorageAccountKey;
  EXTENSIONAPI static const core::Property SASToken;
  EXTENSIONAPI static const core::Property CommonStorageAccountEndpointSuffix;
  EXTENSIONAPI static const core::Property ConnectionString;
  EXTENSIONAPI static const core::Property UseManagedIdentityCredentials;
  static auto properties() {
    return std::array{
      StorageAccountName,
      StorageAccountKey,
      SASToken,
      CommonStorageAccountEndpointSuffix,
      ConnectionString,
      UseManagedIdentityCredentials
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  explicit AzureStorageCredentialsService(std::string name, const minifi::utils::Identifier& uuid = {})
      : ControllerService(std::move(name), uuid) {
  }

  explicit AzureStorageCredentialsService(std::string name, const std::shared_ptr<Configure>& /*configuration*/)
      : ControllerService(std::move(name)) {
  }

  void initialize() override;

  void yield() override {
  }

  bool isWorkAvailable() override {
    return false;
  }

  bool isRunning() override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  void onEnable() override;

  storage::AzureStorageCredentials getCredentials() const {
    return credentials_;
  }

 private:
  storage::AzureStorageCredentials credentials_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AzureStorageCredentialsService>::getLogger();
};

}  // namespace org::apache::nifi::minifi::azure::controllers
