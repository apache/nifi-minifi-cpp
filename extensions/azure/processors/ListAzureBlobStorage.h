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
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::azure::processors {

namespace azure {
SMART_ENUM(EntityTracking,
  (NONE, "none"),
  (TIMESTAMPS, "timestamps")
)
}  // namespace azure

class ListAzureBlobStorage final : public AzureBlobStorageProcessorBase {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Lists blobs in an Azure Storage container. Listing details are attached to an empty FlowFile for use with FetchAzureBlobStorage.";

  EXTENSIONAPI static constexpr auto ListingStrategy = core::PropertyDefinitionBuilder<azure::EntityTracking::length>::createProperty("Listing Strategy")
      .withDescription("Specify how to determine new/updated entities. If 'timestamps' is selected it tracks the latest timestamp of listed entity to determine new/updated entities. "
          "If 'none' is selected it lists an entity without any tracking, the same entity will be listed each time on executing this processor.")
      .isRequired(true)
      .withDefaultValue(toStringView(azure::EntityTracking::TIMESTAMPS))
      .withAllowedValues(azure::EntityTracking::values)
      .build();
  EXTENSIONAPI static constexpr auto Prefix = core::PropertyDefinitionBuilder<>::createProperty("Prefix")
      .withDescription("Search prefix for listing")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureBlobStorageProcessorBase::Properties, std::array<core::PropertyReference, 2>{
      ListingStrategy,
      Prefix
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All FlowFiles that are received are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit ListAzureBlobStorage(std::string name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : ListAzureBlobStorage(std::move(name), nullptr, uuid) {
  }

  explicit ListAzureBlobStorage(std::string name, std::unique_ptr<storage::BlobStorageClient> blob_storage_client, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : AzureBlobStorageProcessorBase(std::move(name), uuid, core::logging::LoggerFactory<ListAzureBlobStorage>::getLogger(uuid), std::move(blob_storage_client)) {
  }

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  std::optional<storage::ListAzureBlobStorageParameters> buildListAzureBlobStorageParameters(core::ProcessContext &context);
  std::shared_ptr<core::FlowFile> createNewFlowFile(core::ProcessSession &session, const storage::ListContainerResultElement &element);

  storage::ListAzureBlobStorageParameters list_parameters_;
  azure::EntityTracking tracking_strategy_ = azure::EntityTracking::TIMESTAMPS;
  std::unique_ptr<minifi::utils::ListingStateManager> state_manager_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
