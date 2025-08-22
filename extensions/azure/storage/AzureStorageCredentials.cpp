/**
 * @file AzureStorageCredentials.cpp
 * AzureStorageCredentials class implementation
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

#include "AzureStorageCredentials.h"

namespace org::apache::nifi::minifi::azure::storage {

void AzureStorageCredentials::setStorageAccountName(const std::string& storage_account_name) {
  storage_account_name_ = storage_account_name;
}

void AzureStorageCredentials::setStorageAccountKey(const std::string& storage_account_key) {
  storage_account_key_ = storage_account_key;
}

void AzureStorageCredentials::setSasToken(const std::string& sas_token) {
  sas_token_ = sas_token;
}

void AzureStorageCredentials::setEndpointSuffix(const std::string& endpoint_suffix) {
  endpoint_suffix_ = endpoint_suffix;
}

void AzureStorageCredentials::setConnectionString(const std::string& connection_string) {
  connection_string_ = connection_string;
}

void AzureStorageCredentials::setCredentialConfigurationStrategy(CredentialConfigurationStrategyOption credential_configuration_strategy) {
  credential_configuration_strategy_ = credential_configuration_strategy;
}

void AzureStorageCredentials::setManagedIdentityClientId(const std::string& managed_identity_client_id) {
  managed_identity_client_id_ = managed_identity_client_id;
}

std::string AzureStorageCredentials::getStorageAccountName() const {
  return storage_account_name_;
}

std::string AzureStorageCredentials::getEndpointSuffix() const {
  return endpoint_suffix_.empty() ? "core.windows.net" : endpoint_suffix_;
}

CredentialConfigurationStrategyOption AzureStorageCredentials::getCredentialConfigurationStrategy() const {
  return credential_configuration_strategy_;
}

std::string AzureStorageCredentials::getManagedIdentityClientId() const {
  return managed_identity_client_id_;
}

std::string AzureStorageCredentials::buildConnectionString() const {
  if (credential_configuration_strategy_ != CredentialConfigurationStrategyOption::fromProperties) {
    return "";
  }

  if (!connection_string_.empty()) {
    return connection_string_;
  }

  if (!storage_account_key_.empty() && !sas_token_.empty()) {
    return "";
  }

  if (storage_account_name_.empty() || (storage_account_key_.empty() && sas_token_.empty())) {
    return "";
  }

  std::string credentials;
  credentials += "AccountName=" + storage_account_name_;

  if (!storage_account_key_.empty()) {
    credentials += ";AccountKey=" + storage_account_key_;
  }

  if (!sas_token_.empty()) {
    credentials += ";SharedAccessSignature=" + (sas_token_[0] == '?' ? sas_token_.substr(1) : sas_token_);
  }

  if (!endpoint_suffix_.empty()) {
    credentials += ";EndpointSuffix=" + endpoint_suffix_;
  }

  return credentials;
}

bool AzureStorageCredentials::isValid() const {
  return (credential_configuration_strategy_ != CredentialConfigurationStrategyOption::fromProperties && !getStorageAccountName().empty()) ||
         (credential_configuration_strategy_ == CredentialConfigurationStrategyOption::fromProperties && !buildConnectionString().empty());
}

std::shared_ptr<Azure::Core::Credentials::TokenCredential> AzureStorageCredentials::createAzureTokenCredential() const {
  std::shared_ptr<Azure::Core::Credentials::TokenCredential> credential;
  if (credential_configuration_strategy_ == CredentialConfigurationStrategyOption::managedIdentity) {
    if (managed_identity_client_id_.empty()) {
      Azure::Identity::ManagedIdentityCredentialOptions options;
      options.IdentityId = Azure::Identity::ManagedIdentityId::SystemAssigned();
      credential = std::make_shared<Azure::Identity::ManagedIdentityCredential>(options);
    } else {
      Azure::Identity::ManagedIdentityCredentialOptions options;
      options.IdentityId = Azure::Identity::ManagedIdentityId::FromUserAssignedClientId(managed_identity_client_id_);
      credential = std::make_shared<Azure::Identity::ManagedIdentityCredential>(options);
    }
  } else if (credential_configuration_strategy_ == CredentialConfigurationStrategyOption::defaultCredential) {
    credential = std::make_shared<Azure::Identity::DefaultAzureCredential>();
  } else if (credential_configuration_strategy_ == CredentialConfigurationStrategyOption::workloadIdentity) {
    credential = std::make_shared<Azure::Identity::WorkloadIdentityCredential>();
  }

  return credential;
}

bool AzureStorageCredentials::operator==(const AzureStorageCredentials& other) const {
  if (other.credential_configuration_strategy_ != credential_configuration_strategy_) {
    return false;
  }

  if (credential_configuration_strategy_ != CredentialConfigurationStrategyOption::fromProperties) {
    return storage_account_name_ == other.storage_account_name_ && endpoint_suffix_ == other.endpoint_suffix_;
  } else {
    return buildConnectionString() == other.buildConnectionString();
  }
}

}  // namespace org::apache::nifi::minifi::azure::storage
