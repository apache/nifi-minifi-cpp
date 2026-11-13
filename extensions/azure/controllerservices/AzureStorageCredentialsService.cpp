// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "AzureStorageCredentialsService.h"

#include <set>

#include "core/Resource.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::azure::controllers {

void AzureStorageCredentialsService::initialize() {
  setSupportedProperties(Properties);
}

void AzureStorageCredentialsService::onEnable() {
  auto credential_configuration_strategy_str = getProperty(CredentialConfigurationStrategy.name).value_or(std::string{magic_enum::enum_name(CredentialConfigurationStrategyOption::FromProperties)});
  if (auto credential_configuration_strategy = magic_enum::enum_cast<CredentialConfigurationStrategyOption>(credential_configuration_strategy_str)) {
    credentials_.setCredentialConfigurationStrategy(*credential_configuration_strategy);
  } else {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Invalid Credential Configuration Strategy: " + credential_configuration_strategy_str);
  }
  if (auto storage_account_name = getProperty(StorageAccountName.name)) {
    credentials_.setStorageAccountName(*storage_account_name);
  }
  if (auto storage_account_key = getProperty(StorageAccountKey.name)) {
    credentials_.setStorageAccountKey(*storage_account_key);
  }
  if (auto sas_token = getProperty(SASToken.name)) {
    credentials_.setSasToken(*sas_token);
  }
  if (auto common_storage_account_endpoint_suffix = getProperty(CommonStorageAccountEndpointSuffix.name)) {
    credentials_.setEndpointSuffix(*common_storage_account_endpoint_suffix);
  }
  if (auto connection_String = getProperty(ConnectionString.name)) {
    credentials_.setConnectionString(*connection_String);
  }
  if (auto managed_identity_client_id = getProperty(ManagedIdentityClientId.name)) {
    credentials_.setManagedIdentityClientId(*managed_identity_client_id);
  }
}

REGISTER_RESOURCE(AzureStorageCredentialsService, ControllerService);

}  // namespace org::apache::nifi::minifi::azure::controllers
