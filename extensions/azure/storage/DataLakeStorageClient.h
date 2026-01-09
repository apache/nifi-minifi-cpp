/**
 * @file DataLakeStorageClient.h
 * DataLakeStorageClient class declaration
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
#include <optional>
#include <memory>
#include <vector>

#include "AzureStorageCredentials.h"

#include "minifi-cpp/utils/gsl.h"
#include "minifi-cpp/io/InputStream.h"
#include "azure/storage/files/datalake/datalake_responses.hpp"
#include "utils/Enum.h"
#include "utils/RegexUtils.h"
#include "minifi-cpp/controllers/ProxyConfigurationServiceInterface.h"
#include "controllers/ProxyConfiguration.h"

namespace org::apache::nifi::minifi::azure::storage {

struct AzureDataLakeStorageParameters {
  AzureStorageCredentials credentials;
  std::string file_system_name;
  std::string directory_name;
  std::optional<uint64_t> number_of_retries;
  std::optional<minifi::controllers::ProxyConfiguration> proxy_configuration;
};

struct AzureDataLakeStorageFileOperationParameters : public AzureDataLakeStorageParameters {
  std::string filename;
};

struct PutAzureDataLakeStorageParameters : public AzureDataLakeStorageFileOperationParameters {
  bool replace_file = false;
};

using DeleteAzureDataLakeStorageParameters = AzureDataLakeStorageFileOperationParameters;

struct FetchAzureDataLakeStorageParameters : public AzureDataLakeStorageFileOperationParameters {
  std::optional<uint64_t> range_start;
  std::optional<uint64_t> range_length;
};

struct ListAzureDataLakeStorageParameters : public AzureDataLakeStorageParameters {
  bool recurse_subdirectories = true;
  std::optional<minifi::utils::Regex> path_regex;
  std::optional<minifi::utils::Regex> file_regex;
};

class DataLakeStorageClient {
 public:
  virtual bool createFile(const PutAzureDataLakeStorageParameters& params) = 0;
  virtual std::string uploadFile(const PutAzureDataLakeStorageParameters& params, std::span<const std::byte> buffer) = 0;
  virtual bool deleteFile(const DeleteAzureDataLakeStorageParameters& params) = 0;
  virtual std::unique_ptr<io::InputStream> fetchFile(const FetchAzureDataLakeStorageParameters& params) = 0;
  virtual std::vector<Azure::Storage::Files::DataLake::Models::PathItem> listDirectory(const ListAzureDataLakeStorageParameters& params) = 0;
  virtual ~DataLakeStorageClient() = default;
};

}  // namespace org::apache::nifi::minifi::azure::storage
