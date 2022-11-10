/**
 * @file AzureBlobStorageSingleBlobProcessorBase.h
 * AzureBlobStorageSingleBlobProcessorBase class declaration
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

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "AzureBlobStorageProcessorBase.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

class AzureBlobStorageSingleBlobProcessorBase : public AzureBlobStorageProcessorBase {
 public:
  EXTENSIONAPI static const core::Property Blob;
  static auto properties() {
    return utils::array_cat(AzureBlobStorageProcessorBase::properties(), std::array{Blob});
  }

  explicit AzureBlobStorageSingleBlobProcessorBase(std::string name, const minifi::utils::Identifier& uuid, const std::shared_ptr<core::logging::Logger>& logger)
    : AzureBlobStorageSingleBlobProcessorBase(std::move(name), uuid, logger, nullptr) {
  }

 protected:
  explicit AzureBlobStorageSingleBlobProcessorBase(
    std::string name,
    const minifi::utils::Identifier& uuid,
    const std::shared_ptr<core::logging::Logger>& logger,
    std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureBlobStorageProcessorBase(std::move(name), uuid, logger, std::move(blob_storage_client)) {
  }

  bool setBlobOperationParameters(
    storage::AzureBlobStorageBlobOperationParameters& params,
    core::ProcessContext &context,
    const std::shared_ptr<core::FlowFile> &flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
