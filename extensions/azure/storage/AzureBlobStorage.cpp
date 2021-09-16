/**
 * @file AzureBlobStorage.cpp
 * AzureBlobStorage class implementation
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

#include "AzureBlobStorage.h"

#include <memory>
#include <utility>

#include "azure/identity.hpp"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace storage {

AzureBlobStorage::AzureBlobStorage(const StorageAccount& storage_account, const std::string& container_name)
  : BlobStorage("", storage_account.name, container_name) {
  auto storage_client = Azure::Storage::Blobs::BlobServiceClient(
      "https://" + storage_account.name + ".blob.core.windows.net", std::make_shared<Azure::Identity::ManagedIdentityCredential>());

  container_client_ = std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(storage_client.GetBlobContainerClient(container_name));
}

AzureBlobStorage::AzureBlobStorage(const ConnectionString& connection_string, std::string container_name)
  : BlobStorage(connection_string.value, "", std::move(container_name))
  , container_client_(std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(
      Azure::Storage::Blobs::BlobContainerClient::CreateFromConnectionString(connection_string_, container_name_))) {
}

void AzureBlobStorage::resetClientIfNeeded(const StorageAccount &storage_account, const std::string &container_name) {
  if (storage_account.name == account_name_ && container_name_ == container_name) {
    logger_->log_debug("Client credentials have not changed, no need to reset client");
    return;
  }
  account_name_ = storage_account.name;
  container_name_ = container_name;
  connection_string_ = "";
  logger_->log_debug("Client has been reset with new credentials");
  auto storage_client = Azure::Storage::Blobs::BlobServiceClient(
      "https://" + storage_account.name + ".blob.core.windows.net", std::make_shared<Azure::Identity::ManagedIdentityCredential>());

  container_client_ = std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(storage_client.GetBlobContainerClient(container_name));
}

void AzureBlobStorage::resetClientIfNeeded(const ConnectionString &connection_string, const std::string &container_name) {
  if (connection_string.value == connection_string_ && container_name_ == container_name) {
    logger_->log_debug("Client credentials have not changed, no need to reset client");
    return;
  }
  connection_string_ = connection_string.value;
  container_name_ = container_name;
  account_name_ = "";
  logger_->log_debug("Client has been reset with new credentials");
  container_client_ = std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(Azure::Storage::Blobs::BlobContainerClient::CreateFromConnectionString(connection_string_, container_name_));
}

void AzureBlobStorage::createContainerIfNotExists() {
  try {
    auto blob_client = container_client_->CreateIfNotExists();
    logger_->log_debug("Container created");
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while creating container: %s", ex.what());
  }
}

std::optional<UploadBlobResult> AzureBlobStorage::uploadBlob(const std::string &blob_name, const uint8_t* buffer, std::size_t buffer_size) {
  try {
    auto blob_client = container_client_->GetBlockBlobClient(blob_name);
    auto response = blob_client.UploadFrom(buffer, buffer_size);

    UploadBlobResult result;
    result.length = buffer_size;
    result.primary_uri = container_client_->GetUrl();
    if (response.Value.ETag.HasValue()) {
      result.etag = response.Value.ETag.ToString();
    }
    result.timestamp = response.Value.LastModified.ToString(Azure::DateTime::DateFormat::Rfc1123);
    return result;
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while uploading blob: %s", ex.what());
    return std::nullopt;
  }
}

}  // namespace storage
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
