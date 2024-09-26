/**
 * @file FetchAzureBlobStorage.h
 * FetchAzureBlobStorage class declaration
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
#include <vector>

#include "core/PropertyDefinition.h"
#include "core/Property.h"
#include "AzureBlobStorageSingleBlobProcessorBase.h"
#include "core/logging/LoggerFactory.h"
#include "utils/ArrayUtils.h"

template<typename T>
class AzureBlobStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class FetchAzureBlobStorage final : public AzureBlobStorageSingleBlobProcessorBase {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Retrieves contents of an Azure Storage Blob, writing the contents to the content of the FlowFile";

  EXTENSIONAPI static constexpr auto RangeStart = core::PropertyDefinitionBuilder<>::createProperty("Range Start")
      .withDescription("The byte position at which to start reading from the blob. An empty value or a value of zero will start reading at the beginning of the blob.")
      .supportsExpressionLanguage(true)
      .build();

  EXTENSIONAPI static constexpr auto RangeLength = core::PropertyDefinitionBuilder<>::createProperty("Range Length")
      .withDescription("The number of bytes to download from the blob, starting from the Range Start. "
                        "An empty value or a value that extends beyond the end of the blob will read to the end of the blob.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureBlobStorageSingleBlobProcessorBase::Properties, std::to_array<core::PropertyReference>({
      RangeStart,
      RangeLength
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All successfully processed FlowFiles are routed to this relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Unsuccessful operations will be transferred to the failure relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit FetchAzureBlobStorage(std::string_view name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : FetchAzureBlobStorage(name, uuid, nullptr) {
  }

  void initialize() override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  friend class ::AzureBlobStorageTestsFixture<FetchAzureBlobStorage>;

  explicit FetchAzureBlobStorage(std::string_view name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureBlobStorageSingleBlobProcessorBase(name, uuid, core::logging::LoggerFactory<FetchAzureBlobStorage>::getLogger(), std::move(blob_storage_client)) {
  }

  std::optional<storage::FetchAzureBlobStorageParameters> buildFetchAzureBlobStorageParameters(
    core::ProcessContext &context, const core::FlowFile& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
