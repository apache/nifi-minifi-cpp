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

void AzureStorageCredentials::setEndpontSuffix(const std::string& endpoint_suffix) {
  endpoint_suffix_ = endpoint_suffix;
}

void AzureStorageCredentials::setConnectionString(const std::string& connection_string) {
  connection_string_ = connection_string;
}

void AzureStorageCredentials::setUseManagedIdentityCredentials(bool use_managed_identity_credentials) {
  use_managed_identity_credentials_ = use_managed_identity_credentials;
}

std::string AzureStorageCredentials::getStorageAccountName() const {
  return storage_account_name_;
}

std::string AzureStorageCredentials::getEndpointSuffix() const {
  return endpoint_suffix_.empty() ? "core.windows.net" : endpoint_suffix_;
}

bool AzureStorageCredentials::getUseManagedIdentityCredentials() const {
  return use_managed_identity_credentials_;
}

std::string AzureStorageCredentials::buildConnectionString() const {
  if (use_managed_identity_credentials_) {
    return "";
  }

  if (!connection_string_.empty()) {
    return connection_string_;
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
  return (getUseManagedIdentityCredentials() && !getStorageAccountName().empty()) ||
         (!getUseManagedIdentityCredentials() && !buildConnectionString().empty());
}

bool AzureStorageCredentials::operator==(const AzureStorageCredentials& other) const {
  if (other.use_managed_identity_credentials_ != use_managed_identity_credentials_) {
    return false;
  }

  if (use_managed_identity_credentials_) {
    return storage_account_name_ == other.storage_account_name_ && endpoint_suffix_ == other.endpoint_suffix_;
  } else {
    return buildConnectionString() == other.buildConnectionString();
  }
}

}  // namespace org::apache::nifi::minifi::azure::storage
