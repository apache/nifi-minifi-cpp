/**
 * @file AzureBlobStorageClient.cpp
 * AzureBlobStorageClient class implementation
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

#include "AzureBlobStorageClient.h"

#include "azure/identity.hpp"

namespace org::apache::nifi::minifi::azure::storage {

void AzureBlobStorageClient::resetClientIfNeeded(const AzureStorageCredentials &credentials, const std::string &container_name) {
  if (container_client_ && credentials == credentials_ && container_name == container_name_) {
    logger_->log_debug("Client credentials have not changed, no need to reset client");
    return;
  }

  if (credentials.getUseManagedIdentityCredentials()) {
    logger_->log_debug("Client has been reset with new managed identity credentials.");
    auto storage_client = Azure::Storage::Blobs::BlobServiceClient(
      "https://" + credentials.getStorageAccountName() + ".blob." + credentials.getEndpointSuffix(), std::make_shared<Azure::Identity::ManagedIdentityCredential>());

    container_client_ = std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(storage_client.GetBlobContainerClient(container_name));
  } else {
    logger_->log_debug("Client has been reset with new connection string credentials.");
    container_client_ = std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(
      Azure::Storage::Blobs::BlobContainerClient::CreateFromConnectionString(credentials.buildConnectionString(), container_name));
  }

  credentials_ = credentials;
  container_name_ = container_name;
}

bool AzureBlobStorageClient::createContainerIfNotExists(const PutAzureBlobStorageParameters& params) {
  resetClientIfNeeded(params.credentials, params.container_name);
  return container_client_->CreateIfNotExists().Value.Created;
}

Azure::Storage::Blobs::Models::UploadBlockBlobResult AzureBlobStorageClient::uploadBlob(const PutAzureBlobStorageParameters& params, gsl::span<const uint8_t> buffer) {
  resetClientIfNeeded(params.credentials, params.container_name);
  auto blob_client = container_client_->GetBlockBlobClient(params.blob_name);
  return blob_client.UploadFrom(buffer.data(), buffer.size()).Value;
}

std::string AzureBlobStorageClient::getUrl(const PutAzureBlobStorageParameters& params) {
  resetClientIfNeeded(params.credentials, params.container_name);
  return container_client_->GetUrl();
}

}  // namespace org::apache::nifi::minifi::azure::storage
