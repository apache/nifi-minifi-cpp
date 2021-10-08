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

#include <utility>

#include "azure/identity.hpp"
#include "azure/storage/blobs/blob_options.hpp"

#include "utils/AzureSdkLogger.h"

namespace org::apache::nifi::minifi::azure::storage {

AzureBlobStorageClient::AzureBlobStorageClient() {
  utils::AzureSdkLogger::initialize();
}

void AzureBlobStorageClient::resetClientIfNeeded(const AzureStorageCredentials &credentials, const std::string &container_name) {
  if (container_client_ && credentials == credentials_ && container_name == container_name_) {
    logger_->log_debug("Azure Blob Storage client credentials have not changed, no need to reset client");
    return;
  }

  if (credentials.getUseManagedIdentityCredentials()) {
    auto storage_client = Azure::Storage::Blobs::BlobServiceClient(
      "https://" + credentials.getStorageAccountName() + ".blob." + credentials.getEndpointSuffix(), std::make_shared<Azure::Identity::ManagedIdentityCredential>());

    container_client_ = std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(storage_client.GetBlobContainerClient(container_name));
    logger_->log_debug("Azure Blob Storage client has been reset with new managed identity credentials.");
  } else {
    container_client_ = std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(
      Azure::Storage::Blobs::BlobContainerClient::CreateFromConnectionString(credentials.buildConnectionString(), container_name));
    logger_->log_debug("Azure Blob Storage client has been reset with new connection string credentials.");
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

bool AzureBlobStorageClient::deleteBlob(const DeleteAzureBlobStorageParameters& params) {
  resetClientIfNeeded(params.credentials, params.container_name);
  Azure::Storage::Blobs::DeleteBlobOptions delete_options;
  if (params.optional_deletion == OptionalDeletion::INCLUDE_SNAPSHOTS) {
    delete_options.DeleteSnapshots = Azure::Storage::Blobs::Models::DeleteSnapshotsOption::IncludeSnapshots;
  } else if (params.optional_deletion == OptionalDeletion::DELETE_SNAPSHOTS_ONLY) {
    delete_options.DeleteSnapshots = Azure::Storage::Blobs::Models::DeleteSnapshotsOption::OnlySnapshots;
  }
  auto response = container_client_->DeleteBlob(params.blob_name, delete_options);
  return response.Value.Deleted;
}

Azure::Storage::Blobs::Models::DownloadBlobResult AzureBlobStorageClient::fetchBlob(const FetchAzureBlobStorageParameters& params) {
  resetClientIfNeeded(params.credentials, params.container_name);
  auto blob_client = container_client_->GetBlobClient(params.blob_name);
  Azure::Storage::Blobs::DownloadBlobOptions options;
  if (params.range_start || params.range_length) {
    Azure::Core::Http::HttpRange range;
    if (params.range_start) {
      range.Offset = *params.range_start;
    }

    if (params.range_length) {
      range.Length = *params.range_length;
    }
    options.Range = range;
  }
  auto result = blob_client.Download(options);
  return std::move(result.Value);
}

}  // namespace org::apache::nifi::minifi::azure::storage
