/**
 * @file AzureBlobStorageClient.h
 * AzureBlobStorageClient class declaration
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
#include <utility>

#include "BlobStorageClient.h"
#include "azure/storage/blobs.hpp"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::azure::storage {

class AzureBlobStorageClient : public BlobStorageClient {
 public:
  AzureBlobStorageClient();
  bool createContainerIfNotExists(const PutAzureBlobStorageParameters& params) override;
  Azure::Storage::Blobs::Models::UploadBlockBlobResult uploadBlob(const PutAzureBlobStorageParameters& params, std::span<const std::byte> buffer) override;
  std::string getUrl(const AzureBlobStorageParameters& params) override;
  bool deleteBlob(const DeleteAzureBlobStorageParameters& params) override;
  std::unique_ptr<io::InputStream> fetchBlob(const FetchAzureBlobStorageParameters& params) override;
  std::vector<Azure::Storage::Blobs::Models::BlobItem> listContainer(const ListAzureBlobStorageParameters& params) override;

 private:
  static std::unique_ptr<Azure::Storage::Blobs::BlobContainerClient> createClient(const AzureStorageCredentials& credentials, const std::string &container_name);

  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AzureBlobStorageClient>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::azure::storage
