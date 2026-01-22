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

#include "core/controller/ControllerServiceBase.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "storage/AzureStorageCredentials.h"
#include "minifi-cpp/utils/Export.h"
#include "utils/AzureEnums.h"

namespace org::apache::nifi::minifi::azure::controllers {

class AzureStorageCredentialsService : public core::controller::ControllerServiceBase, public core::controller::ControllerServiceInterface {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Manages the credentials for an Azure Storage account. This allows for multiple Azure Storage related processors to reference this single "
      "controller service so that Azure storage credentials can be managed and controlled in a central location.";

  EXTENSIONAPI static constexpr auto StorageAccountName = core::PropertyDefinitionBuilder<>::createProperty("Storage Account Name")
      .withDescription("The storage account name.")
      .build();
  EXTENSIONAPI static constexpr auto StorageAccountKey = core::PropertyDefinitionBuilder<>::createProperty("Storage Account Key")
      .withDescription("The storage account key. This is an admin-like password providing access to every container in this account. "
          "It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies "
          "if Credential Configuration Strategy is set to From Properties. If set, SAS Token must be empty.")
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto SASToken = core::PropertyDefinitionBuilder<>::createProperty("SAS Token")
      .withDescription("Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name "
          "if Credential Configuration Strategy is set to From Properties. If set, Storage Account Key must be empty.")
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto CommonStorageAccountEndpointSuffix = core::PropertyDefinitionBuilder<>::createProperty("Common Storage Account Endpoint Suffix")
      .withDescription("Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a "
          "different suffix in certain circumstances (like Azure Stack or non-public Azure regions).")
      .build();
  EXTENSIONAPI static constexpr auto ConnectionString = core::PropertyDefinitionBuilder<>::createProperty("Connection String")
      .withDescription("Connection string used to connect to Azure Storage service. This overrides all other set credential properties "
          "if Credential Configuration Strategy is set to From Properties.")
      .build();
  EXTENSIONAPI static constexpr auto CredentialConfigurationStrategy =
    core::PropertyDefinitionBuilder<magic_enum::enum_count<CredentialConfigurationStrategyOption>()>::createProperty("Credential Configuration Strategy")
      .withDescription("The strategy to use for credential configuration. If set to From Properties, the credentials are parsed from the SAS Token, Storage Account Key, "
          "and Connection String properties. In other cases, the selected Azure identity source is used.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(CredentialConfigurationStrategyOption::FromProperties))
      .withAllowedValues(magic_enum::enum_names<CredentialConfigurationStrategyOption>())
      .build();
  EXTENSIONAPI static constexpr auto ManagedIdentityClientId = core::PropertyDefinitionBuilder<>::createProperty("Managed Identity Client ID")
      .withDescription("Client ID of the managed identity. The property is required when User Assigned Managed Identity is used for authentication and multiple user-assigned identities "
          "are added to the resource. It must be empty in case of System Assigned Managed Identity and can also be left empty if only one user-assigned identity is present.")
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      StorageAccountName,
      StorageAccountKey,
      SASToken,
      CommonStorageAccountEndpointSuffix,
      ConnectionString,
      CredentialConfigurationStrategy,
      ManagedIdentityClientId
  });

  using ControllerServiceBase::ControllerServiceBase;

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;

  void initialize() override;

  void onEnable() override;

  ControllerServiceInterface* getControllerServiceInterface() override {return this;}

  storage::AzureStorageCredentials getCredentials() const {
    return credentials_;
  }

 private:
  storage::AzureStorageCredentials credentials_;
};

}  // namespace org::apache::nifi::minifi::azure::controllers
