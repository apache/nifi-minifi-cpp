/**
 * @file AzureBlobStorageProcessorBase.h
 * AzureBlobStorageProcessorBase class declaration
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

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/Property.h"
#include "core/PropertyDefinition.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "core/logging/Logger.h"
#include "storage/AzureBlobStorage.h"
#include "AzureStorageProcessorBase.h"
#include "storage/AzureStorageCredentials.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

class AzureBlobStorageProcessorBase : public AzureStorageProcessorBase {
 public:
  EXTENSIONAPI static constexpr auto ContainerName = core::PropertyDefinitionBuilder<>::createProperty("Container Name")
      .withDescription("Name of the Azure Storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.")
      .supportsExpressionLanguage(true)
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto StorageAccountName = core::PropertyDefinitionBuilder<>::createProperty("Storage Account Name")
      .withDescription("The storage account name.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto StorageAccountKey = core::PropertyDefinitionBuilder<>::createProperty("Storage Account Key")
      .withDescription("The storage account key. This is an admin-like password providing access to every container in this account. "
                       "It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies "
                       "if Credential Configuration Strategy is set to From Properties. If set, SAS Token must be empty.")
      .supportsExpressionLanguage(true)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto SASToken = core::PropertyDefinitionBuilder<>::createProperty("SAS Token")
      .withDescription("Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name "
                       "if Credential Configuration Strategy is set to From Properties. If set, Storage Account Key must be empty.")
      .supportsExpressionLanguage(true)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto CommonStorageAccountEndpointSuffix = core::PropertyDefinitionBuilder<>::createProperty("Common Storage Account Endpoint Suffix")
      .withDescription("Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a "
                       "different suffix in certain circumstances (like Azure Stack or non-public Azure regions).")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ConnectionString = core::PropertyDefinitionBuilder<>::createProperty("Connection String")
      .withDescription("Connection string used to connect to Azure Storage service. This overrides all other set credential properties "
                       "if Credential Configuration Strategy is set to From Properties.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto CredentialConfigurationStrategy =
    core::PropertyDefinitionBuilder<magic_enum::enum_count<CredentialConfigurationStrategyOption>()>::createProperty("Credential Configuration Strategy")
      .withDescription("The strategy to use for credential configuration. If set to From Properties, the credentials are parsed from the SAS Token, Storage Account Key, "
                       "and Connection String properties. In other cases, the selected Azure identity source is used.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(CredentialConfigurationStrategyOption::fromProperties))
      .withAllowedValues(magic_enum::enum_names<CredentialConfigurationStrategyOption>())
      .build();
  EXTENSIONAPI static constexpr auto ManagedIdentityClientId = core::PropertyDefinitionBuilder<>::createProperty("Managed Identity Client ID")
      .withDescription("Client ID of the managed identity. The property is required when User Assigned Managed Identity is used for authentication and multiple user-assigned identities "
                       "are added to the resource. It must be empty in case of System Assigned Managed Identity and can also be left empty if only one user-assigned identity is present.")
      .isSensitive(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureStorageProcessorBase::Properties, std::to_array<core::PropertyReference>({
      ContainerName,
      StorageAccountName,
      StorageAccountKey,
      SASToken,
      CommonStorageAccountEndpointSuffix,
      ConnectionString,
      CredentialConfigurationStrategy,
      ManagedIdentityClientId
  }));


  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  explicit AzureBlobStorageProcessorBase(
    core::ProcessorMetadata metadata,
    std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureStorageProcessorBase(metadata),
      azure_blob_storage_(std::move(blob_storage_client)) {
  }

  storage::AzureStorageCredentials getAzureCredentialsFromProperties(
    core::ProcessContext &context, const core::FlowFile* const flow_file) const;
  std::optional<storage::AzureStorageCredentials> getCredentials(
    core::ProcessContext &context, const core::FlowFile* const flow_file) const;
  bool setCommonStorageParameters(
    storage::AzureBlobStorageParameters& params,
    core::ProcessContext &context,
    const core::FlowFile* const flow_file);

  storage::AzureBlobStorage azure_blob_storage_;
  CredentialConfigurationStrategyOption credential_configuration_strategy_ = CredentialConfigurationStrategyOption::fromProperties;
};

}  // namespace org::apache::nifi::minifi::azure::processors
