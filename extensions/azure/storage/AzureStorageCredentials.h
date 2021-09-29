/**
 * @file AzureStorageCredentials.h
 * AzureStorageCredentials class declaration
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

namespace org::apache::nifi::minifi::azure::storage {

class AzureStorageCredentials {
 public:
  void setStorageAccountName(const std::string& storage_account_name);
  void setStorageAccountKey(const std::string& storage_account_key);
  void setSasToken(const std::string& sas_token);
  void setEndpontSuffix(const std::string& endpoint_suffix);
  void setConnectionString(const std::string& connection_string);
  void setUseManagedIdentityCredentials(bool use_managed_identity_credentials);

  std::string getStorageAccountName() const;
  std::string getEndpointSuffix() const;
  bool getUseManagedIdentityCredentials() const;
  std::string buildConnectionString() const;
  bool isValid() const;

  bool operator==(const AzureStorageCredentials& other) const;

 private:
  std::string storage_account_name_;
  std::string storage_account_key_;
  std::string sas_token_;
  std::string endpoint_suffix_;
  std::string connection_string_;
  bool use_managed_identity_credentials_ = false;
};

}  // namespace org::apache::nifi::minifi::azure::storage
