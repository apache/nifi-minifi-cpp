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

void AzureDataLakeStorageClient::resetClientIfNeeded(const AzureStorageCredentials& credentials, const std::string& file_system_name) {
  if (client_ && credentials_ == credentials && file_system_name_ == file_system_name) {
    logger_->log_debug("Azure Data Lake Storge client credentials have not changed, no need to reset client");
    return;
  }

  if (credentials.getUseManagedIdentityCredentials()) {
    auto datalake_service_client = Azure::Storage::Files::DataLake::DataLakeServiceClient(
      "https://" + credentials.getStorageAccountName() + ".dfs." + credentials.getEndpointSuffix(), std::make_shared<Azure::Identity::ManagedIdentityCredential>());

    client_ = std::make_unique<Azure::Storage::Files::DataLake::DataLakeFileSystemClient>(datalake_service_client.GetFileSystemClient(file_system_name));
    logger_->log_debug("Azure Data Lake Storge client has been reset with new managed identity credentials.");
  } else {
    client_ = std::make_unique<Azure::Storage::Files::DataLake::DataLakeFileSystemClient>(
    Azure::Storage::Files::DataLake::DataLakeFileSystemClient::CreateFromConnectionString(credentials.buildConnectionString(), file_system_name));
    logger_->log_debug("Azure Data Lake Storge client has been reset with new connection string credentials.");
  }

  file_system_name_ = file_system_name;
  credentials_ = credentials;
}

Azure::Storage::Files::DataLake::DataLakeFileClient AzureDataLakeStorageClient::getFileClient(const PutAzureDataLakeStorageParameters& params) {
  resetClientIfNeeded(params.credentials, params.file_system_name);

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
