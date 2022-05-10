/**
 * @file BlobStorageClient.h
 * BlobStorageClient class declaration
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
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <memory>

#include "azure/storage/blobs/protocol/blob_rest_client.hpp"
#include "AzureStorageCredentials.h"
#include "utils/gsl.h"
#include "utils/Enum.h"
#include "io/InputStream.h"

namespace org::apache::nifi::minifi::azure::storage {

SMART_ENUM(OptionalDeletion,
  (NONE, "None"),
  (INCLUDE_SNAPSHOTS, "Include Snapshots"),
  (DELETE_SNAPSHOTS_ONLY, "Delete Snapshots Only")
)

struct AzureBlobStorageParameters {
  AzureStorageCredentials credentials;
  std::string container_name;
};

struct AzureBlobStorageBlobOperationParameters : public AzureBlobStorageParameters {
  std::string blob_name;
};

using PutAzureBlobStorageParameters = AzureBlobStorageBlobOperationParameters;

struct DeleteAzureBlobStorageParameters : public AzureBlobStorageBlobOperationParameters {
  OptionalDeletion optional_deletion;
};

struct FetchAzureBlobStorageParameters : public AzureBlobStorageBlobOperationParameters {
  std::optional<uint64_t> range_start;
  std::optional<uint64_t> range_length;
};

struct ListAzureBlobStorageParameters : public AzureBlobStorageParameters {
  std::string prefix;
};

class BlobStorageClient {
 public:
  virtual bool createContainerIfNotExists(const PutAzureBlobStorageParameters& params) = 0;
  virtual Azure::Storage::Blobs::Models::UploadBlockBlobResult uploadBlob(const PutAzureBlobStorageParameters& params, gsl::span<const std::byte> buffer) = 0;
  virtual std::string getUrl(const AzureBlobStorageParameters& params) = 0;
  virtual bool deleteBlob(const DeleteAzureBlobStorageParameters& params) = 0;
  virtual std::unique_ptr<io::InputStream> fetchBlob(const FetchAzureBlobStorageParameters& params) = 0;
  virtual std::vector<Azure::Storage::Blobs::Models::BlobItem> listContainer(const ListAzureBlobStorageParameters& params) = 0;
  virtual ~BlobStorageClient() = default;
};

}  // namespace org::apache::nifi::minifi::azure::storage
