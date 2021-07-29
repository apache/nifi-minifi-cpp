/**
 * @file AzureDataLakeStorage.cpp
 * AzureDataLakeStorage class implementation
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

#include "AzureDataLakeStorage.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace storage {

std::optional<UploadDataLakeStorageResult> AzureDataLakeStorage::uploadFile(const PutAzureDataLakeStorageParameters& params, const uint8_t* buffer, std::size_t buffer_size) {
  auto file_created = data_lake_storage_client_->createFile(params);
  if (!file_created) {
    return std::nullopt;
  }
  if (!file_created.value() && !params.replace_file) {
    std::string message = "File " + params.filename + " already exists on Azure Data Lake Storage";
    logger_->log_error(message.c_str());
    throw FileAlreadyExistsException("File " + params.filename + " already exists on Azure Data Lake Storage");
  }

  auto upload_url = data_lake_storage_client_->uploadFile(params, buffer, buffer_size);
  if (!upload_url) {
    return std::nullopt;
  }

  UploadDataLakeStorageResult result;
  result.length = buffer_size;
  result.primary_uri = upload_url.value();
  return result;
}

}  // namespace storage
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
