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

#include "azure/storage/blobs/blob_responses.hpp"
#include "AzureStorageCredentials.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/Enum.h"
#include "minifi-cpp/io/InputStream.h"
#include "minifi-cpp/controllers/ProxyConfigurationServiceInterface.h"

namespace org::apache::nifi::minifi::azure::storage {

enum class OptionalDeletion {
  NONE,
  INCLUDE_SNAPSHOTS,
  DELETE_SNAPSHOTS_ONLY
};
}  // namespace org::apache::nifi::minifi::azure::storage

namespace magic_enum::customize {
using OptionalDeletion = org::apache::nifi::minifi::azure::storage::OptionalDeletion;

template <>
constexpr customize_t enum_name<OptionalDeletion>(OptionalDeletion value) noexcept {
  switch (value) {
    case OptionalDeletion::NONE:
      return "None";
    case OptionalDeletion::INCLUDE_SNAPSHOTS:
      return "Include Snapshots";
    case OptionalDeletion::DELETE_SNAPSHOTS_ONLY:
      return "Delete Snapshots Only";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::azure::storage {

struct AzureBlobStorageParameters {
  AzureStorageCredentials credentials;
  std::string container_name;
  std::optional<minifi::controllers::ProxyConfiguration> proxy_configuration;
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
  virtual Azure::Storage::Blobs::Models::UploadBlockBlobResult uploadBlob(const PutAzureBlobStorageParameters& params, std::span<const std::byte> buffer) = 0;
  virtual std::string getUrl(const AzureBlobStorageParameters& params) = 0;
  virtual bool deleteBlob(const DeleteAzureBlobStorageParameters& params) = 0;
  virtual std::unique_ptr<io::InputStream> fetchBlob(const FetchAzureBlobStorageParameters& params) = 0;
  virtual std::vector<Azure::Storage::Blobs::Models::BlobItem> listContainer(const ListAzureBlobStorageParameters& params) = 0;
  virtual ~BlobStorageClient() = default;
};

}  // namespace org::apache::nifi::minifi::azure::storage
