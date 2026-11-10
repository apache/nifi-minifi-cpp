// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <utility>
#include <memory>

#include "AzureDataLakeStorageFileProcessorBase.h"

template<typename AzureDataLakeStorageProcessorBase>
class AzureDataLakeStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class DeleteAzureDataLakeStorage final : public AzureDataLakeStorageFileProcessorBase {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Deletes the provided file from Azure Data Lake Storage";

  EXTENSIONAPI static constexpr auto Properties = AzureDataLakeStorageFileProcessorBase::Properties;

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "If file deletion from Azure storage succeeds the flowfile is transferred to this relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "If file deletion from Azure storage fails the flowfile is transferred to this relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  using AzureDataLakeStorageFileProcessorBase::AzureDataLakeStorageFileProcessorBase;

  ~DeleteAzureDataLakeStorage() override = default;

  void initialize() override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  friend class ::AzureDataLakeStorageTestsFixture<DeleteAzureDataLakeStorage>;

  explicit DeleteAzureDataLakeStorage(core::ProcessorMetadata metadata, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageFileProcessorBase(metadata, std::move(data_lake_storage_client)) {
  }

  std::optional<storage::DeleteAzureDataLakeStorageParameters> buildDeleteParameters(core::ProcessContext& context, const core::FlowFile& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
