// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <memory>

#include "utils/AzureEnums.h"
#include "azure/identity.hpp"

namespace org::apache::nifi::minifi::azure::storage {

class AzureStorageCredentials {
 public:
  void setStorageAccountName(const std::string& storage_account_name);
  void setStorageAccountKey(const std::string& storage_account_key);
  void setSasToken(const std::string& sas_token);
  void setEndpointSuffix(const std::string& endpoint_suffix);
  void setConnectionString(const std::string& connection_string);
  void setCredentialConfigurationStrategy(CredentialConfigurationStrategyOption credential_configuration_strategy);
  void setManagedIdentityClientId(const std::string& managed_identity_client_id);

  std::string getStorageAccountName() const;
  std::string getEndpointSuffix() const;
  CredentialConfigurationStrategyOption getCredentialConfigurationStrategy() const;
  std::string getManagedIdentityClientId() const;
  std::string buildConnectionString() const;
  bool isValid() const;

  std::shared_ptr<Azure::Core::Credentials::TokenCredential> createAzureTokenCredential() const;

  bool operator==(const AzureStorageCredentials& other) const;

 private:
  std::string storage_account_name_;
  std::string storage_account_key_;
  std::string sas_token_;
  std::string endpoint_suffix_;
  std::string connection_string_;
  std::string managed_identity_client_id_;
  CredentialConfigurationStrategyOption credential_configuration_strategy_ = CredentialConfigurationStrategyOption::FromProperties;
};

}  // namespace org::apache::nifi::minifi::azure::storage
