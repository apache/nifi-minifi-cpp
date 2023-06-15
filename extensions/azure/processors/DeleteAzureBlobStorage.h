/**
 * @file DeleteAzureBlobStorage.h
 * DeleteAzureBlobStorage class declaration
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

#include "core/Property.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "AzureBlobStorageSingleBlobProcessorBase.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/ArrayUtils.h"

template<typename T>
class AzureBlobStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class DeleteAzureBlobStorage final : public AzureBlobStorageSingleBlobProcessorBase {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Deletes the provided blob from Azure Storage";

  EXTENSIONAPI static constexpr auto DeleteSnapshotsOption = core::PropertyDefinitionBuilder<storage::OptionalDeletion::length>::createProperty("Delete Snapshots Option")
      .withDescription("Specifies the snapshot deletion options to be used when deleting a blob. None: Deletes the blob only. Include Snapshots: Delete the blob and its snapshots. "
          "Delete Snapshots Only: Delete only the blob's snapshots.")
      .isRequired(true)
      .withDefaultValue(toStringView(storage::OptionalDeletion::NONE))
      .withAllowedValues(storage::OptionalDeletion::values)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureBlobStorageSingleBlobProcessorBase::Properties, std::array<core::PropertyReference, 1>{DeleteSnapshotsOption});

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All successfully processed FlowFiles are routed to this relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Unsuccessful operations will be transferred to the failure relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit DeleteAzureBlobStorage(std::string name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : DeleteAzureBlobStorage(std::move(name), uuid, nullptr) {
  }

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::AzureBlobStorageTestsFixture<DeleteAzureBlobStorage>;

  explicit DeleteAzureBlobStorage(std::string name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureBlobStorageSingleBlobProcessorBase(std::move(name), uuid, core::logging::LoggerFactory<DeleteAzureBlobStorage>::getLogger(), std::move(blob_storage_client)) {
  }

  std::optional<storage::DeleteAzureBlobStorageParameters> buildDeleteAzureBlobStorageParameters(
    core::ProcessContext &context, const std::shared_ptr<core::FlowFile> &flow_file);

  storage::OptionalDeletion optional_deletion_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
