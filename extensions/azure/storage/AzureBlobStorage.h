/**
 * @file AzureBlobStorage.h
 * AzureBlobStorage class declaration
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

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "BlobStorageClient.h"
#include "azure/storage/blobs.hpp"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::azure::storage {

struct UploadBlobResult {
  std::string primary_uri;
  std::string etag;
  std::string timestamp;
};

class AzureBlobStorage {
 public:
  explicit AzureBlobStorage(std::unique_ptr<BlobStorageClient> blob_storage_client = nullptr);
  std::optional<bool> createContainerIfNotExists(const PutAzureBlobStorageParameters& params);
  std::optional<UploadBlobResult> uploadBlob(const PutAzureBlobStorageParameters& params, gsl::span<const std::byte> buffer);
  bool deleteBlob(const DeleteAzureBlobStorageParameters& params);
  std::optional<uint64_t> fetchBlob(const FetchAzureBlobStorageParameters& params, io::BaseStream& stream);

 private:
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AzureBlobStorage>::getLogger()};
  gsl::not_null<std::unique_ptr<BlobStorageClient>> blob_storage_client_;
};

}  // namespace org::apache::nifi::minifi::azure::storage
