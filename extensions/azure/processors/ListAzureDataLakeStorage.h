/**
 * @file ListAzureDataLakeStorage.h
 * ListAzureDataLakeStorage class declaration
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

#include <string>
#include <utility>
#include <memory>

#include "AzureDataLakeStorageProcessorBase.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "utils/ArrayUtils.h"
#include "utils/AzureEnums.h"

class ListAzureDataLakeStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class ListAzureDataLakeStorage final : public AzureDataLakeStorageProcessorBase {
 public:
  EXTENSIONAPI static constexpr const char *Description = "Lists directory in an Azure Data Lake Storage Gen 2 filesystem";

  EXTENSIONAPI static constexpr auto RecurseSubdirectories = core::PropertyDefinitionBuilder<>::createProperty("Recurse Subdirectories")
      .withDescription("Indicates whether to list files from subdirectories of the directory")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto FileFilter = core::PropertyDefinitionBuilder<>::createProperty("File Filter")
      .withDescription("Only files whose names match the given regular expression will be listed")
      .build();
  EXTENSIONAPI static constexpr auto PathFilter = core::PropertyDefinitionBuilder<>::createProperty("Path Filter")
      .withDescription("When 'Recurse Subdirectories' is true, then only subdirectories whose paths match the given regular expression will be scanned")
      .build();
  EXTENSIONAPI static constexpr auto ListingStrategy = core::PropertyDefinitionBuilder<magic_enum::enum_count<azure::EntityTracking>()>::createProperty("Listing Strategy")
      .withDescription("Specify how to determine new/updated entities. If 'timestamps' is selected it tracks the latest timestamp of listed entity to "
          "determine new/updated entities. If 'none' is selected it lists an entity without any tracking, the same entity will be listed each time on executing this processor.")
      .withDefaultValue(magic_enum::enum_name(azure::EntityTracking::timestamps))
      .withAllowedValues(magic_enum::enum_names<azure::EntityTracking>())
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureDataLakeStorageProcessorBase::Properties, std::to_array<core::PropertyReference>({
      RecurseSubdirectories,
      FileFilter,
      PathFilter,
      ListingStrategy
  }));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All FlowFiles that are received are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit ListAzureDataLakeStorage(std::string_view name, const minifi::utils::Identifier &uuid = minifi::utils::Identifier())
      : AzureDataLakeStorageProcessorBase(name, uuid, core::logging::LoggerFactory<ListAzureDataLakeStorage>::getLogger(uuid)) {
  }

  ~ListAzureDataLakeStorage() override = default;

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  friend class ::ListAzureDataLakeStorageTestsFixture;

  explicit ListAzureDataLakeStorage(std::string_view name, const minifi::utils::Identifier &uuid, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
      : AzureDataLakeStorageProcessorBase(name, uuid, core::logging::LoggerFactory<ListAzureDataLakeStorage>::getLogger(uuid), std::move(data_lake_storage_client)) {
  }

  std::optional<storage::ListAzureDataLakeStorageParameters> buildListParameters(core::ProcessContext &context);

  azure::EntityTracking tracking_strategy_ = azure::EntityTracking::timestamps;
  storage::ListAzureDataLakeStorageParameters list_parameters_;
  std::unique_ptr<minifi::utils::ListingStateManager> state_manager_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
