/**
 * @file AzureDataLakeStorage.h
 * AzureDataLakeStorage class declaration
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

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <utility>
#include <string_view>

#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "DataLakeStorageClient.h"
#include "azure/core/io/body_stream.hpp"

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

struct ListDataLakeStorageElement {
    std::string filesystem;
    std::string file_path;
    std::string directory;
    std::string filename;
    uint64_t length = 0;
    uint64_t last_modified = 0;
    std::string etag;
};

using ListDataLakeStorageResult = std::vector<ListDataLakeStorageElement>;

class AzureDataLakeStorage {
 public:
  explicit AzureDataLakeStorage(std::unique_ptr<DataLakeStorageClient> data_lake_storage_client = nullptr);

  storage::UploadDataLakeStorageResult uploadFile(const storage::PutAzureDataLakeStorageParameters& params, gsl::span<const uint8_t> buffer);
  bool deleteFile(const storage::DeleteAzureDataLakeStorageParameters& params);
  std::optional<uint64_t> fetchFile(const FetchAzureDataLakeStorageParameters& params, io::BaseStream& stream);
  std::optional<ListDataLakeStorageResult> listDirectory(const ListAzureDataLakeStorageParameters& params);

 private:
  bool matchesPathFilter(const std::string& base_directory, const std::string& path_filter, std::string path);
  bool matchesFileFilter(const std::string& file_filter, const std::string& filename);
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AzureDataLakeStorage>::getLogger()};
  std::unique_ptr<DataLakeStorageClient> data_lake_storage_client_;
};

}  // namespace org::apache::nifi::minifi::azure::storage
