// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <utility>

#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "DataLakeStorageClient.h"
#include "utils/ListingStateManager.h"
#include "minifi-cpp/io/OutputStream.h"

namespace org::apache::nifi::minifi::azure::storage {

enum class UploadResultCode {
  SUCCESS,
  FILE_ALREADY_EXISTS,
  FAILURE
};

struct UploadDataLakeStorageResult {
  UploadResultCode result_code = UploadResultCode::SUCCESS;
  std::string primary_uri;
};

struct ListDataLakeStorageElement : public minifi::utils::ListedObject {
  std::string filesystem;
  std::string file_path;
  std::filesystem::path directory;
  std::filesystem::path filename;
  uint64_t length = 0;
  std::chrono::time_point<std::chrono::system_clock> last_modified;
  std::string etag;

  std::chrono::time_point<std::chrono::system_clock> getLastModified() const override {
    return last_modified;
  }

  std::string getKey() const override {
    return file_path;
  }
};

using ListDataLakeStorageResult = std::vector<ListDataLakeStorageElement>;

class AzureDataLakeStorage {
 public:
  explicit AzureDataLakeStorage(std::unique_ptr<DataLakeStorageClient> data_lake_storage_client = nullptr);

  storage::UploadDataLakeStorageResult uploadFile(const storage::PutAzureDataLakeStorageParameters& params, std::span<const std::byte> buffer);
  bool deleteFile(const storage::DeleteAzureDataLakeStorageParameters& params);
  std::optional<uint64_t> fetchFile(const FetchAzureDataLakeStorageParameters& params, io::OutputStream& stream);
  std::optional<ListDataLakeStorageResult> listDirectory(const ListAzureDataLakeStorageParameters& params);

 private:
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AzureDataLakeStorage>::getLogger()};
  std::unique_ptr<DataLakeStorageClient> data_lake_storage_client_;
};

}  // namespace org::apache::nifi::minifi::azure::storage
