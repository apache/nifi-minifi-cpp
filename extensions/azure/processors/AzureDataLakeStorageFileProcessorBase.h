// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <utility>
#include <memory>

#include "AzureDataLakeStorageProcessorBase.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

class AzureDataLakeStorageFileProcessorBase : public AzureDataLakeStorageProcessorBase {
 public:
  EXTENSIONAPI static constexpr auto FileName = core::PropertyDefinitionBuilder<>::createProperty("File Name")
      .withDescription("The filename in Azure Storage. If left empty the filename attribute will be used by default.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureDataLakeStorageProcessorBase::Properties, std::to_array<core::PropertyReference>({FileName}));

  using AzureDataLakeStorageProcessorBase::AzureDataLakeStorageProcessorBase;

  ~AzureDataLakeStorageFileProcessorBase() override = default;

 protected:
  explicit AzureDataLakeStorageFileProcessorBase(core::ProcessorMetadata metadata, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageProcessorBase(metadata, std::move(data_lake_storage_client)) {
  }

  bool setFileOperationCommonParameters(
    storage::AzureDataLakeStorageFileOperationParameters& params,
    core::ProcessContext& context,
    const core::FlowFile& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
