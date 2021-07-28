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
#include "core/Resource.h"

#include <set>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace controllers {

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
      ->withDescription("Shared Access Signature token. Specify either SAS Token (recommended) or Account Key.")
      ->build());
const core::Property AzureStorageCredentialsService::CommonStorageAccountEndpointSuffix(
    core::PropertyBuilder::createProperty("Common Storage Account Endpoint Suffix")
      ->withDescription("Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a "
                        "different suffix in certain circumstances (like Azure Stack or non-public Azure regions).")
      ->build());
const core::Property AzureStorageCredentialsService::ConnectionString(
  core::PropertyBuilder::createProperty("Connection String")
    ->withDescription("Connection string used to connect to Azure Storage service. This overrides all other set credential properties.")
    ->build());

void AzureStorageCredentialsService::initialize() {
  setSupportedProperties({StorageAccountName, StorageAccountKey, SASToken, CommonStorageAccountEndpointSuffix, ConnectionString});
}

void AzureStorageCredentialsService::onEnable() {
  getProperty(StorageAccountName.getName(), credentials_.storage_account_name);
  getProperty(StorageAccountKey.getName(), credentials_.storage_account_key);
  getProperty(SASToken.getName(), credentials_.sas_token);
  getProperty(CommonStorageAccountEndpointSuffix.getName(), credentials_.endpoint_suffix);
  getProperty(ConnectionString.getName(), credentials_.connection_string);
}

REGISTER_RESOURCE(AzureStorageCredentialsService, "Azure Storage Credentials Management Service");

}  // namespace controllers
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
