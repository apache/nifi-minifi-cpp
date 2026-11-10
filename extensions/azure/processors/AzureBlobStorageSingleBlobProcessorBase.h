// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "AzureBlobStorageProcessorBase.h"
#include "minifi-cpp/core/PropertyDefinition.h"
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
    core::ProcessorMetadata metadata,
    std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureBlobStorageProcessorBase(metadata, std::move(blob_storage_client)) {
  }

  bool setBlobOperationParameters(
    storage::AzureBlobStorageBlobOperationParameters& params,
    core::ProcessContext& context,
    const core::FlowFile& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
