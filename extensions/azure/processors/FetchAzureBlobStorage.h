// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "minifi-cpp/core/PropertyDefinition.h"
#include "minifi-cpp/core/Property.h"
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

  explicit FetchAzureBlobStorage(core::ProcessorMetadata metadata)
    : FetchAzureBlobStorage(metadata, nullptr) {
  }

  void initialize() override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  friend class ::AzureBlobStorageTestsFixture<FetchAzureBlobStorage>;

  explicit FetchAzureBlobStorage(core::ProcessorMetadata metadata, std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureBlobStorageSingleBlobProcessorBase(metadata, std::move(blob_storage_client)) {
  }

  std::optional<storage::FetchAzureBlobStorageParameters> buildFetchAzureBlobStorageParameters(
    core::ProcessContext &context, const core::FlowFile& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
