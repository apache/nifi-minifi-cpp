// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <utility>

#include "BlobStorageClient.h"
#include "azure/storage/blobs.hpp"
#include "minifi-cpp/core/logging/Logger.h"
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
  static Azure::Storage::Blobs::BlobContainerClient createClient(const AzureStorageCredentials& credentials, const std::string &container_name,
    const minifi::controllers::ProxyConfiguration& proxy_configuration);

  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AzureBlobStorageClient>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::azure::storage
