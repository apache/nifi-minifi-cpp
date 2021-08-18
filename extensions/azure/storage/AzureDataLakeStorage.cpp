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

namespace org::apache::nifi::minifi::azure::storage {

AzureDataLakeStorage::AzureDataLakeStorage()
  : data_lake_storage_client_(std::make_unique<AzureDataLakeStorageClient>()) {
}

AzureDataLakeStorage::AzureDataLakeStorage(std::unique_ptr<DataLakeStorageClient> data_lake_storage_client)
  : data_lake_storage_client_(std::move(data_lake_storage_client)) {
}

UploadDataLakeStorageResult AzureDataLakeStorage::uploadFile(const PutAzureDataLakeStorageParameters& params, const uint8_t* buffer, std::size_t buffer_size) {
  auto file_created = data_lake_storage_client_->createFile(params);
  if (!file_created && !params.replace_file) {
    throw FileAlreadyExistsException(params);
  }

  auto upload_url = data_lake_storage_client_->uploadFile(params, buffer, buffer_size);
  UploadDataLakeStorageResult result;
  result.length = buffer_size;
  result.primary_uri = upload_url;
  return result;
}

}  // namespace org::apache::nifi::minifi::azure::storage
