/**
 * @file BlobStorageWrapper.cpp
 * BlobStorageWrapper class implementation
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

#include "BlobStorageWrapper.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace storage {

BlobStorageWrapper::BlobStorageWrapper(const std::string &connection_string, const std::string &container_name)
  : container_client_(Azure::Storage::Blobs::BlobContainerClient::CreateFromConnectionString(connection_string, container_name)) {
}

bool BlobStorageWrapper::uploadBlob(const std::string &blob_name, const std::vector<uint8_t> &buffer) {
  try
  {
    auto blob_client = container_client_.GetBlockBlobClient(blob_name);
    Azure::Core::Http::MemoryBodyStream data(buffer);
    blob_client.Upload(&data);
    return true;
  } catch (const std::runtime_error&) {
    return false;
  }
}

}  // namespace storage
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
