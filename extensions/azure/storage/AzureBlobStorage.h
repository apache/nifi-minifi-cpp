/**
 * @file AzureBlobStorage.h
 * AzureBlobStorage class declaration
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

#include "BlobStorage.h"
#include "azure/storage/blobs/blob.hpp"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace storage {

class AzureBlobStorage : public BlobStorage {
 public:
  AzureBlobStorage(const std::string &connection_string, const std::string &container_name);
  void createContainer() override;
  void resetClientIfNeeded(const std::string &connection_string, const std::string &container_name) override;
  utils::optional<UploadBlobResult> uploadBlob(const std::string &blob_name, const uint8_t* buffer, std::size_t buffer_size) override;

 private:
  std::unique_ptr<Azure::Storage::Blobs::BlobContainerClient> container_client_;
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<AzureBlobStorage>::getLogger()};
};

}  // namespace storage
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
