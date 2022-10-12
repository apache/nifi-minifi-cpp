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
#include "AzureBlobStorageClient.h"
#include "io/StreamPipe.h"

namespace org::apache::nifi::minifi::azure::storage {

AzureBlobStorage::AzureBlobStorage(std::unique_ptr<BlobStorageClient> blob_storage_client)
  : blob_storage_client_(blob_storage_client ? std::move(blob_storage_client) : std::make_unique<AzureBlobStorageClient>()) {
}

std::optional<bool> AzureBlobStorage::createContainerIfNotExists(const PutAzureBlobStorageParameters& params) {
  try {
    logger_->log_debug("Trying to create Azure blob container %s", params.container_name);
    return blob_storage_client_->createContainerIfNotExists(params);
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while creating container: %s", ex.what());
    return std::nullopt;
  }
}

std::optional<UploadBlobResult> AzureBlobStorage::uploadBlob(const PutAzureBlobStorageParameters& params, gsl::span<const std::byte> buffer) {
  try {
    logger_->log_debug("Uploading Azure blob %s to container %s", params.blob_name, params.container_name);
    auto response = blob_storage_client_->uploadBlob(params, buffer);

    UploadBlobResult result;
    auto upload_url = blob_storage_client_->getUrl(params);
    if (auto query_string_pos = upload_url.find('?'); query_string_pos != std::string::npos) {
      upload_url = upload_url.substr(0, query_string_pos);
    }
    result.primary_uri = upload_url;
    if (response.ETag.HasValue()) {
      result.etag = response.ETag.ToString();
    }
    result.timestamp = response.LastModified.ToString(Azure::DateTime::DateFormat::Rfc1123);
    return result;
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while uploading blob: %s", ex.what());
    return std::nullopt;
  }
}

bool AzureBlobStorage::deleteBlob(const DeleteAzureBlobStorageParameters& params) {
  try {
    blob_storage_client_->deleteBlob(params);
    return true;
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while deleting blob: %s", ex.what());
    return false;
  }
}

std::optional<uint64_t> AzureBlobStorage::fetchBlob(const FetchAzureBlobStorageParameters& params, io::OutputStream& stream) {
  try {
    auto fetch_res = blob_storage_client_->fetchBlob(params);
    return internal::pipe(*fetch_res, stream);
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while fetching blob '%s' of container '%s': %s", params.blob_name, params.container_name, ex.what());
    return std::nullopt;
  }
}

std::optional<ListContainerResult> AzureBlobStorage::listContainer(const ListAzureBlobStorageParameters& params) {
  try {
    ListContainerResult result;
    auto blobs = blob_storage_client_->listContainer(params);
    auto primary_uri = blob_storage_client_->getUrl(params);
    for (const auto& blob : blobs) {
      ListContainerResultElement element;
      element.blob_name = blob.Name;
      element.primary_uri = primary_uri;
      element.etag = blob.Details.ETag.ToString();
      element.length = blob.BlobSize;
      element.last_modified = static_cast<std::chrono::system_clock::time_point>(blob.Details.LastModified);
      element.mime_type = blob.Details.HttpHeaders.ContentType;
      element.language = blob.Details.HttpHeaders.ContentLanguage;
      element.blob_type = blob.BlobType.ToString();
      result.push_back(element);
    }
    return result;
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while listing container: %s", ex.what());
    return std::nullopt;
  }
}

}  // namespace org::apache::nifi::minifi::azure::storage
