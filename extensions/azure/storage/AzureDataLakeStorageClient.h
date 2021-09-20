/**
 * @file AzureDataLakeStorageClient.h
 * AzureDataLakeStorageClient class declaration
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
#include <memory>

#include <azure/storage/files/datalake.hpp>

#include "DataLakeStorageClient.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "AzureStorageEntities.h"

namespace org::apache::nifi::minifi::azure::storage {

class AzureDataLakeStorageClient : public DataLakeStorageClient {
 public:
  /**
   * Creates a file on Azure Data Lake Storage
   * @param params Parameters required for connecting and file creation on Azure
   * @return True if a new file was created, false otherwise
   */
  bool createFile(const PutAzureDataLakeStorageParameters& params) override;

  /**
   * Creates a file on the Azure Data Lake Storage
   * @param params Parameters required for connecting and file access on Azure
   * @param buffer Buffer containing the data to be uploaded
   * @return URI of the file uploaded
   */
  std::string uploadFile(const PutAzureDataLakeStorageParameters& params, gsl::span<const uint8_t> buffer) override;

 private:
  void resetClientIfNeeded(const ConnectionString& connection_string, const std::string& file_system_name);
  void resetClientIfNeeded(const ManagedIdentityParameters& managed_identity_params, const std::string& file_system_name);
  Azure::Storage::Files::DataLake::DataLakeFileClient getFileClient(const PutAzureDataLakeStorageParameters& params);

  std::string connection_string_;
  std::string storage_account_;
  std::string endpoint_suffix_;
  std::string file_system_name_;
  std::unique_ptr<Azure::Storage::Files::DataLake::DataLakeFileSystemClient> client_;
};

}  // namespace org::apache::nifi::minifi::azure::storage
