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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace storage {

struct UploadDataLakeStorageResult {
  std::string primary_uri;
  std::size_t length;
};

class AzureDataLakeStorage {
 public:
  class FileAlreadyExistsException : public std::runtime_error {
   public:
    explicit FileAlreadyExistsException(const PutAzureDataLakeStorageParameters& params)
      : std::runtime_error("File '" + params.directory_name + "/" + params.filename + "' already exists on Azure Data Lake Storage filesystem '" + params.file_system_name + "'") {}
  };

  AzureDataLakeStorage();
  explicit AzureDataLakeStorage(std::unique_ptr<DataLakeStorageClient> data_lake_storage_client);

  azure::storage::UploadDataLakeStorageResult uploadFile(const storage::PutAzureDataLakeStorageParameters& params, const uint8_t* buffer, std::size_t buffer_size);

 private:
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<AzureDataLakeStorage>::getLogger()};
  std::unique_ptr<DataLakeStorageClient> data_lake_storage_client_;
};

}  // namespace storage
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
