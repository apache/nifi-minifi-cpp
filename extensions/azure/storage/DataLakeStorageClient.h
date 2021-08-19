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

#include "AzureStorageCredentials.h"

#include "utils/gsl.h"
#include "io/InputStream.h"

namespace org::apache::nifi::minifi::azure::storage {

struct AzureDataLakeStorageParameters {
  AzureStorageCredentials credentials;
  std::string file_system_name;
  std::string directory_name;
  std::string filename;
  std::optional<uint64_t> number_of_retries;
};

struct PutAzureDataLakeStorageParameters : public AzureDataLakeStorageParameters {
  bool replace_file = false;
};

using DeleteAzureDataLakeStorageParameters = AzureDataLakeStorageParameters;

struct FetchAzureDataLakeStorageParameters : public AzureDataLakeStorageParameters {
  std::optional<uint64_t> range_start;
  std::optional<uint64_t> range_length;
};

class DataLakeStorageClient {
 public:
  virtual bool createFile(const PutAzureDataLakeStorageParameters& params) = 0;
  virtual std::string uploadFile(const PutAzureDataLakeStorageParameters& params, gsl::span<const uint8_t> buffer) = 0;
  virtual bool deleteFile(const DeleteAzureDataLakeStorageParameters& params) = 0;
  virtual std::unique_ptr<io::InputStream> fetchFile(const FetchAzureDataLakeStorageParameters& params) = 0;
  virtual ~DataLakeStorageClient() = default;
};

}  // namespace org::apache::nifi::minifi::azure::storage
