/**
 * @file ListAzureBlobStorage.h
 * ListAzureBlobStorage class declaration
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
#include "AzureBlobStorageProcessorBase.h"
#include "core/logging/LoggerFactory.h"
#include "utils/AzureEnums.h"

namespace org::apache::nifi::minifi::azure::processors {

class ListAzureBlobStorage final : public AzureBlobStorageProcessorBase {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Lists blobs in an Azure Storage container. Listing details are attached to an empty FlowFile for use with FetchAzureBlobStorage.";

  EXTENSIONAPI static constexpr auto ListingStrategy = core::PropertyDefinitionBuilder<magic_enum::enum_count<EntityTracking>()>::createProperty("Listing Strategy")
      .withDescription("Specify how to determine new/updated entities. If 'timestamps' is selected it tracks the latest timestamp of listed entity to determine new/updated entities. "
          "If 'none' is selected it lists an entity without any tracking, the same entity will be listed each time on executing this processor.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(EntityTracking::timestamps))
      .withAllowedValues(magic_enum::enum_names<EntityTracking>())
      .build();
  EXTENSIONAPI static constexpr auto Prefix = core::PropertyDefinitionBuilder<>::createProperty("Prefix")
      .withDescription("Search prefix for listing")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureBlobStorageProcessorBase::Properties, std::to_array<core::PropertyReference>({
      ListingStrategy,
      Prefix
  }));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All FlowFiles that are received are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit ListAzureBlobStorage(std::string_view name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : ListAzureBlobStorage(name, nullptr, uuid) {
  }

  explicit ListAzureBlobStorage(std::string_view name, std::unique_ptr<storage::BlobStorageClient> blob_storage_client, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : AzureBlobStorageProcessorBase(name, uuid, core::logging::LoggerFactory<ListAzureBlobStorage>::getLogger(uuid), std::move(blob_storage_client)) {
  }

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::optional<storage::ListAzureBlobStorageParameters> buildListAzureBlobStorageParameters(core::ProcessContext &context);
  std::shared_ptr<core::FlowFile> createNewFlowFile(core::ProcessSession &session, const storage::ListContainerResultElement &element);

  storage::ListAzureBlobStorageParameters list_parameters_;
  azure::EntityTracking tracking_strategy_ = azure::EntityTracking::timestamps;
  std::unique_ptr<minifi::utils::ListingStateManager> state_manager_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
