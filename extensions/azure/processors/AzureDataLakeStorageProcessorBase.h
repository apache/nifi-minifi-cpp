// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <utility>
#include <string>
#include <memory>
#include <optional>

#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "storage/AzureDataLakeStorage.h"
#include "utils/ArrayUtils.h"
#include "AzureStorageProcessorBase.h"

namespace org::apache::nifi::minifi::azure::processors {

class AzureDataLakeStorageProcessorBase : public AzureStorageProcessorBase {
 public:
  EXTENSIONAPI static constexpr auto FilesystemName = core::PropertyDefinitionBuilder<>::createProperty("Filesystem Name")
      .withDescription("Name of the Azure Storage File System. It is assumed to be already existing.")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto DirectoryName = core::PropertyDefinitionBuilder<>::createProperty("Directory Name")
      .withDescription("Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. "
          "If left empty it designates the root directory. The directory will be created if not already existing.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureStorageProcessorBase::Properties, std::to_array<core::PropertyReference>({
      FilesystemName,
      DirectoryName
  }));

  using AzureStorageProcessorBase::AzureStorageProcessorBase;

  ~AzureDataLakeStorageProcessorBase() override = default;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  explicit AzureDataLakeStorageProcessorBase(core::ProcessorMetadata metadata, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureStorageProcessorBase(metadata),
      azure_data_lake_storage_(std::move(data_lake_storage_client)) {
  }

  bool setCommonParameters(storage::AzureDataLakeStorageParameters& params, core::ProcessContext& context, const core::FlowFile* const flow_file);

  storage::AzureStorageCredentials credentials_;
  storage::AzureDataLakeStorage azure_data_lake_storage_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
