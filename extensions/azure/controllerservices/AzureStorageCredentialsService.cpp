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

#include "AzureStorageCredentialsService.h"

#include <set>

#include "core/Resource.h"

namespace org::apache::nifi::minifi::azure::controllers {

void AzureStorageCredentialsService::initialize() {
  setSupportedProperties(Properties);
}

void AzureStorageCredentialsService::onEnable() {
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
    credentials_.setEndpontSuffix(*common_storage_account_endpoint_suffix);
  }
  if (auto connection_String = getProperty(ConnectionString.name)) {
    credentials_.setConnectionString(*connection_String);
  }
  if (auto use_managed_identity_credentials = getProperty(UseManagedIdentityCredentials.name) | utils::andThen(parsing::parseBool)) {
    credentials_.setUseManagedIdentityCredentials(*use_managed_identity_credentials);
  }
}

REGISTER_RESOURCE(AzureStorageCredentialsService, ControllerService);

}  // namespace org::apache::nifi::minifi::azure::controllers
