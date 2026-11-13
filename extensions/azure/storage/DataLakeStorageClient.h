// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
  minifi::controllers::ProxyConfiguration proxy_configuration;
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
