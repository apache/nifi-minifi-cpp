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
#include "core/PropertyDefinition.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

class AzureBlobStorageSingleBlobProcessorBase : public AzureBlobStorageProcessorBase {
 public:
  EXTENSIONAPI static constexpr auto Blob = core::PropertyDefinitionBuilder<>::createProperty("Blob")
      .withDescription("The filename of the blob. If left empty the filename attribute will be used by default.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureBlobStorageProcessorBase::Properties, std::to_array<core::PropertyReference>({Blob}));

 protected:
  explicit AzureBlobStorageSingleBlobProcessorBase(
    std::string_view name,
    const minifi::utils::Identifier& uuid,
    const std::shared_ptr<core::logging::Logger>& logger,
    std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureBlobStorageProcessorBase(name, uuid, logger, std::move(blob_storage_client)) {
  }

  bool setBlobOperationParameters(
    storage::AzureBlobStorageBlobOperationParameters& params,
    core::ProcessContext& context,
    const core::FlowFile& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
