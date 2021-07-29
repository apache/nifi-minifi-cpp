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

#include <optional>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace storage {

struct PutAzureDataLakeStorageParameters {
  std::string connection_string;
  std::string file_system_name;
  std::string directory_name;
  std::string filename;
  bool replace_file = false;
};

class DataLakeStorageClient {
 public:
  virtual std::optional<bool> createFile(const PutAzureDataLakeStorageParameters& params) = 0;
  virtual std::optional<std::string> uploadFile(const PutAzureDataLakeStorageParameters& params, const uint8_t* buffer, std::size_t buffer_size) = 0;
};

}  // namespace storage
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
