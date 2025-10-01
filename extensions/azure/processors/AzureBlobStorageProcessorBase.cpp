/**
 * @file AzureBlobStorageProcessorBase.cpp
 * AzureBlobStorageProcessorBase class implementation
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

#include "AzureBlobStorageProcessorBase.h"

#include "minifi-cpp/core/ProcessContext.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

void AzureBlobStorageProcessorBase::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  AzureStorageProcessorBase::onSchedule(context, session_factory);
  if (!context.hasNonEmptyProperty(ContainerName.name)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Container Name property missing or invalid");
  }

  if (auto azure_storage_credentials_service = context.getProperty(AzureStorageCredentialsService.name)) {
    logger_->log_info("Getting Azure Storage credentials from controller service with name: '{}'", *azure_storage_credentials_service);
    return;
  }

  credential_configuration_strategy_ = minifi::utils::parseEnumProperty<CredentialConfigurationStrategyOption>(context, CredentialConfigurationStrategy);

  if (credential_configuration_strategy_ != CredentialConfigurationStrategyOption::FromProperties) {
    logger_->log_info("Using {} for authentication", magic_enum::enum_name(credential_configuration_strategy_));
    return;
  }

  if (context.hasNonEmptyProperty(ConnectionString.name)) {
    logger_->log_info("Using connectionstring directly for Azure Storage authentication");
    return;
  }

  if (!context.hasNonEmptyProperty(StorageAccountName.name)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Storage Account Name property missing or invalid");
  }

  if (context.hasNonEmptyProperty(StorageAccountKey.name)) {
    logger_->log_info("Using storage account name and key for authentication");
    return;
  }

  if (!context.hasNonEmptyProperty(SASToken.name)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Neither Storage Account Key nor SAS Token property was set.");
  }

  logger_->log_info("Using storage account name and SAS token for authentication");
}

storage::AzureStorageCredentials AzureBlobStorageProcessorBase::getAzureCredentialsFromProperties(
    core::ProcessContext &context, const core::FlowFile* const flow_file) const {
  storage::AzureStorageCredentials credentials;
  credentials.setCredentialConfigurationStrategy(credential_configuration_strategy_);
  if (const auto value = context.getProperty(StorageAccountName, flow_file)) {
    credentials.setStorageAccountName(*value);
  }
  if (const auto value = context.getProperty(StorageAccountKey, flow_file)) {
    credentials.setStorageAccountKey(*value);
  }
  if (const auto value = context.getProperty(SASToken, flow_file)) {
    credentials.setSasToken(*value);
  }
  if (const auto value = context.getProperty(CommonStorageAccountEndpointSuffix, flow_file)) {
    credentials.setEndpointSuffix(*value);
  }
  if (const auto value = context.getProperty(ConnectionString, flow_file)) {
    credentials.setConnectionString(*value);
  }
  if (const auto value = context.getProperty(ManagedIdentityClientId, flow_file)) {
    credentials.setManagedIdentityClientId(*value);
  }
  return credentials;
}

bool AzureBlobStorageProcessorBase::setCommonStorageParameters(
    storage::AzureBlobStorageParameters& params,
    core::ProcessContext &context,
    const core::FlowFile* const flow_file) {
  auto credentials = getCredentials(context, flow_file);
  if (!credentials) {
    return false;
  }

  params.credentials = *credentials;

  params.container_name = context.getProperty(ContainerName, flow_file).value_or("");
  if (params.container_name.empty()) {
    logger_->log_error("Container Name is invalid or empty!");
    return false;
  }

  params.proxy_configuration = proxy_configuration_;

  return true;
}

std::optional<storage::AzureStorageCredentials> AzureBlobStorageProcessorBase::getCredentials(
    core::ProcessContext &context,
    const core::FlowFile* const flow_file) const {
  auto [result, controller_service_creds] = getCredentialsFromControllerService(context);
  if (controller_service_creds) {
    if (controller_service_creds->isValid()) {
      logger_->log_debug("Azure credentials read from credentials controller service!");
      return controller_service_creds;
    } else {
      logger_->log_error("Azure credentials controller service is set with invalid credential parameters!");
      return std::nullopt;
    }
  } else if (result == GetCredentialsFromControllerResult::CONTROLLER_NAME_INVALID) {
    logger_->log_error("Azure credentials controller service name is invalid!");
    return std::nullopt;
  }

  logger_->log_debug("No valid Azure credentials are set in credentials controller service, checking properties...");

  auto property_creds = getAzureCredentialsFromProperties(context, flow_file);
  if (property_creds.isValid()) {
    logger_->log_debug("Azure credentials read from properties!");
    return property_creds;
  }

  logger_->log_error("No valid Azure credentials are set in credentials controller service nor in properties!");
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::azure::processors
