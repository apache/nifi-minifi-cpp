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
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "storage/AzureStorageCredentials.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::azure::controllers {

class AzureStorageCredentialsService : public core::controller::ControllerServiceImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Manages the credentials for an Azure Storage account. This allows for multiple Azure Storage related processors to reference this single "
      "controller service so that Azure storage credentials can be managed and controlled in a central location.";

  EXTENSIONAPI static constexpr auto StorageAccountName = core::PropertyDefinitionBuilder<>::createProperty("Storage Account Name")
      .withDescription("The storage account name.")
      .build();
  EXTENSIONAPI static constexpr auto StorageAccountKey = core::PropertyDefinitionBuilder<>::createProperty("Storage Account Key")
      .withDescription("The storage account key. This is an admin-like password providing access to every container in this account. "
          "It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies.")
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto SASToken = core::PropertyDefinitionBuilder<>::createProperty("SAS Token")
      .withDescription("Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Managed Identity is not used.")
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto CommonStorageAccountEndpointSuffix = core::PropertyDefinitionBuilder<>::createProperty("Common Storage Account Endpoint Suffix")
      .withDescription("Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a "
          "different suffix in certain circumstances (like Azure Stack or non-public Azure regions).")
      .build();
  EXTENSIONAPI static constexpr auto ConnectionString = core::PropertyDefinitionBuilder<>::createProperty("Connection String")
      .withDescription("Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Managed Identity is not used.")
      .build();
  EXTENSIONAPI static constexpr auto UseManagedIdentityCredentials = core::PropertyDefinitionBuilder<>::createProperty("Use Managed Identity Credentials")
      .withDescription("If true Managed Identity credentials will be used together with the Storage Account Name for authentication.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      StorageAccountName,
      StorageAccountKey,
      SASToken,
      CommonStorageAccountEndpointSuffix,
      ConnectionString,
      UseManagedIdentityCredentials
  });


  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  explicit AzureStorageCredentialsService(std::string_view name, const minifi::utils::Identifier& uuid = {})
      : ControllerServiceImpl(name, uuid) {
  }

  explicit AzureStorageCredentialsService(std::string_view name, const std::shared_ptr<Configure>& /*configuration*/)
      : ControllerServiceImpl(name) {
  }

  void initialize() override;

  void yield() override {
  }

  bool isWorkAvailable() override {
    return false;
  }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  void onEnable() override;

  storage::AzureStorageCredentials getCredentials() const {
    return credentials_;
  }

 private:
  storage::AzureStorageCredentials credentials_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AzureStorageCredentialsService>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::azure::controllers
