/**
 * @file AzureDataLakeStorageClient.cpp
 * AzureDataLakeStorageClient class implementation
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

#include "AzureDataLakeStorageClient.h"

#include "azure/identity.hpp"

namespace org::apache::nifi::minifi::azure::storage {

void AzureDataLakeStorageClient::resetClientIfNeeded(const ConnectionString& connection_string, const std::string& file_system_name) {
  if (client_ == nullptr || connection_string_ != connection_string.value || file_system_name_ != file_system_name) {
    client_ = std::make_unique<Azure::Storage::Files::DataLake::DataLakeFileSystemClient>(
      Azure::Storage::Files::DataLake::DataLakeFileSystemClient::CreateFromConnectionString(connection_string.value, file_system_name));
    file_system_name_ = file_system_name;
    connection_string_ = connection_string.value;
    storage_account_ = "";
  }
}

void AzureDataLakeStorageClient::resetClientIfNeeded(const StorageAccount& storage_account, const std::string& file_system_name) {
  if (client_ == nullptr || storage_account_ != storage_account.name || file_system_name_ != file_system_name) {
    auto datalake_service_client = Azure::Storage::Files::DataLake::DataLakeServiceClient(
      "https://" + storage_account.name + ".dfs.core.windows.net", std::make_shared<Azure::Identity::ManagedIdentityCredential>());

    client_ = std::make_unique<Azure::Storage::Files::DataLake::DataLakeFileSystemClient>(datalake_service_client.GetFileSystemClient(file_system_name));
    file_system_name_ = file_system_name;
    storage_account_ = storage_account.name;
    connection_string_ = "";
  }
}

Azure::Storage::Files::DataLake::DataLakeFileClient AzureDataLakeStorageClient::getFileClient(const PutAzureDataLakeStorageParameters& params) {
  if (params.connection_string.empty()) {
    resetClientIfNeeded(StorageAccount{params.account_name}, params.file_system_name);
  } else {
    resetClientIfNeeded(ConnectionString{params.connection_string}, params.file_system_name);
  }

  auto directory_client = client_->GetDirectoryClient(params.directory_name);
  if (!params.directory_name.empty()) {
    directory_client.CreateIfNotExists();
  }
  return directory_client.GetFileClient(params.filename);
}

bool AzureDataLakeStorageClient::createFile(const PutAzureDataLakeStorageParameters& params) {
  auto file_client = getFileClient(params);
  auto response = file_client.CreateIfNotExists();
  return response.Value.Created;
}

std::string AzureDataLakeStorageClient::uploadFile(const PutAzureDataLakeStorageParameters& params, gsl::span<const uint8_t> buffer) {
  auto file_client = getFileClient(params);
  file_client.UploadFrom(buffer.data(), buffer.size());
  return file_client.GetUrl();
}

}  // namespace org::apache::nifi::minifi::azure::storage
