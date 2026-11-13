// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "AzureBlobStorageSingleBlobProcessorBase.h"
#include "core/logging/LoggerFactory.h"
#include "utils/ArrayUtils.h"

template<typename T>
class AzureBlobStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class DeleteAzureBlobStorage final : public AzureBlobStorageSingleBlobProcessorBase {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Deletes the provided blob from Azure Storage";

  EXTENSIONAPI static constexpr auto DeleteSnapshotsOption = core::PropertyDefinitionBuilder<magic_enum::enum_count<storage::OptionalDeletion>()>::createProperty("Delete Snapshots Option")
      .withDescription("Specifies the snapshot deletion options to be used when deleting a blob. None: Deletes the blob only. Include Snapshots: Delete the blob and its snapshots. "
          "Delete Snapshots Only: Delete only the blob's snapshots.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(storage::OptionalDeletion::NONE))
      .withAllowedValues(magic_enum::enum_names<storage::OptionalDeletion>())
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureBlobStorageSingleBlobProcessorBase::Properties, std::to_array<core::PropertyReference>({DeleteSnapshotsOption}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All successfully processed FlowFiles are routed to this relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Unsuccessful operations will be transferred to the failure relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit DeleteAzureBlobStorage(core::ProcessorMetadata metadata)
    : DeleteAzureBlobStorage(metadata, nullptr) {
  }

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  friend class ::AzureBlobStorageTestsFixture<DeleteAzureBlobStorage>;

  explicit DeleteAzureBlobStorage(core::ProcessorMetadata metadata, std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureBlobStorageSingleBlobProcessorBase(metadata, std::move(blob_storage_client)) {
  }

  std::optional<storage::DeleteAzureBlobStorageParameters> buildDeleteAzureBlobStorageParameters(
    core::ProcessContext &context, const core::FlowFile& flow_file);

  storage::OptionalDeletion optional_deletion_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
