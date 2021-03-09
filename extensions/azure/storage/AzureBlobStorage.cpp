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

#include <utility>

#include "utils/GeneralUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace storage {

AzureBlobStorage::AzureBlobStorage(std::string connection_string, std::string container_name)
  : BlobStorage(std::move(connection_string), std::move(container_name))
  , container_client_(minifi::utils::make_unique<Azure::Storage::Blobs::BlobContainerClient>(
      Azure::Storage::Blobs::BlobContainerClient::CreateFromConnectionString(connection_string_, container_name_))) {
}

void AzureBlobStorage::resetClientIfNeeded(const std::string &connection_string, const std::string &container_name) {
  if (connection_string == connection_string_ && container_name_ == container_name) {
    logger_->log_debug("Client credentials have not changed, no need to reset client");
    return;
  }
  connection_string_ = connection_string;
  container_name_ = container_name;
  logger_->log_debug("Client has been reset with new credentials");
  container_client_ = minifi::utils::make_unique<Azure::Storage::Blobs::BlobContainerClient>(Azure::Storage::Blobs::BlobContainerClient::CreateFromConnectionString(connection_string, container_name));
}

void AzureBlobStorage::createContainer() {
  try {
    auto blob_client = container_client_->Create();
    logger_->log_debug("Container created");
  } catch (const std::runtime_error&) {
    logger_->log_debug("Container creation failed, it already exists.");
  }
}

utils::optional<UploadBlobResult> AzureBlobStorage::uploadBlob(const std::string &blob_name, const uint8_t* buffer, std::size_t buffer_size) {
  try {
    auto blob_client = container_client_->GetBlockBlobClient(blob_name);
    auto response = blob_client.UploadFrom(buffer, buffer_size);
    if (!response.HasValue()) {
      return utils::nullopt;
    }

    UploadBlobResult result;
    result.length = buffer_size;
    result.primary_uri = container_client_->GetUrl();
    if (response->ETag.HasValue()) {
      result.etag = response->ETag.ToString();
    }
    result.timestamp = response->LastModified.GetString(Azure::Core::DateTime::DateFormat::Rfc1123);
    return result;
  } catch (const std::runtime_error& err) {
    logger_->log_error("A runtime error occurred while uploading blob: %s", err.what());
    return utils::nullopt;
  }
}

}  // namespace storage
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
