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

#include "core/PropertyBuilder.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::azure::controllers {

const core::Property AzureStorageCredentialsService::StorageAccountName(
    core::PropertyBuilder::createProperty("Storage Account Name")
      ->withDescription("The storage account name.")
      ->build());
const core::Property AzureStorageCredentialsService::StorageAccountKey(
    core::PropertyBuilder::createProperty("Storage Account Key")
      ->withDescription("The storage account key. This is an admin-like password providing access to every container in this account. "
                        "It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies.")
      ->build());
const core::Property AzureStorageCredentialsService::SASToken(
    core::PropertyBuilder::createProperty("SAS Token")
      ->withDescription("Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Managed Identity is not used.")
      ->build());
const core::Property AzureStorageCredentialsService::CommonStorageAccountEndpointSuffix(
    core::PropertyBuilder::createProperty("Common Storage Account Endpoint Suffix")
      ->withDescription("Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a "
                        "different suffix in certain circumstances (like Azure Stack or non-public Azure regions).")
      ->build());
const core::Property AzureStorageCredentialsService::ConnectionString(
  core::PropertyBuilder::createProperty("Connection String")
    ->withDescription("Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Managed Identity is not used.")
    ->build());
const core::Property AzureStorageCredentialsService::UseManagedIdentityCredentials(
  core::PropertyBuilder::createProperty("Use Managed Identity Credentials")
    ->withDescription("If true Managed Identity credentials will be used together with the Storage Account Name for authentication.")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->build());

void AzureStorageCredentialsService::initialize() {
  setSupportedProperties(properties());
}

void AzureStorageCredentialsService::onEnable() {
  std::string value;
  if (getProperty(StorageAccountName.getName(), value)) {
    credentials_.setStorageAccountName(value);
  }
  if (getProperty(StorageAccountKey.getName(), value)) {
    credentials_.setStorageAccountKey(value);
  }
  if (getProperty(SASToken.getName(), value)) {
    credentials_.setSasToken(value);
  }
  if (getProperty(CommonStorageAccountEndpointSuffix.getName(), value)) {
    credentials_.setEndpontSuffix(value);
  }
  if (getProperty(ConnectionString.getName(), value)) {
    credentials_.setConnectionString(value);
  }
  bool use_managed_identity_credentials = false;
  if (getProperty(UseManagedIdentityCredentials.getName(), use_managed_identity_credentials)) {
    credentials_.setUseManagedIdentityCredentials(use_managed_identity_credentials);
  }
}

REGISTER_RESOURCE(AzureStorageCredentialsService, ControllerService);

}  // namespace org::apache::nifi::minifi::azure::controllers
