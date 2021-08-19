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

#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "AzureDataLakeStorageClient.h"

namespace org::apache::nifi::minifi::azure::storage {

enum class UploadResultCode {
  SUCCESS,
  FILE_ALREADY_EXISTS,
  FAILURE
};

struct UploadDataLakeStorageResult {
  UploadResultCode result_code = UploadResultCode::SUCCESS;
  std::string primary_uri;
  std::size_t length;
};

class AzureDataLakeStorage {
 public:
  AzureDataLakeStorage();
  explicit AzureDataLakeStorage(std::unique_ptr<DataLakeStorageClient> data_lake_storage_client);

  azure::storage::UploadDataLakeStorageResult uploadFile(const storage::PutAzureDataLakeStorageParameters& params, const uint8_t* buffer, std::size_t buffer_size);

 private:
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<AzureDataLakeStorage>::getLogger()};
  std::unique_ptr<DataLakeStorageClient> data_lake_storage_client_;
};

}  // namespace org::apache::nifi::minifi::azure::storage
