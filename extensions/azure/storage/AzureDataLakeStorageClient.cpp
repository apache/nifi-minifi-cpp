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

namespace org::apache::nifi::minifi::azure::storage {

void AzureDataLakeStorageClient::resetClientIfNeeded(const std::string& connection_string, const std::string& file_system_name) {
  if (client_ == nullptr || connection_string_ != connection_string || file_system_name_ != file_system_name) {
    client_ = std::make_unique<Azure::Storage::Files::DataLake::DataLakeFileSystemClient>(
      Azure::Storage::Files::DataLake::DataLakeFileSystemClient::CreateFromConnectionString(connection_string, file_system_name));
    file_system_name_ = file_system_name;
    connection_string_ = connection_string;
  }
}

Azure::Storage::Files::DataLake::DataLakeFileClient AzureDataLakeStorageClient::getFileClient(const PutAzureDataLakeStorageParameters& params) {
  resetClientIfNeeded(params.connection_string, params.file_system_name);
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

std::string AzureDataLakeStorageClient::uploadFile(const PutAzureDataLakeStorageParameters& params, const uint8_t* buffer, std::size_t buffer_size) {
  auto file_client = getFileClient(params);
  file_client.UploadFrom(buffer, buffer_size);
  return file_client.GetUrl();
}

}  // namespace org::apache::nifi::minifi::azure::storage
