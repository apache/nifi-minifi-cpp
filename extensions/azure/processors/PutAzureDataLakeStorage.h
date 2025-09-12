/**
 * @file PutAzureDataLakeStorage.h
 * PutAzureDataLakeStorage class declaration
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

#include "AzureDataLakeStorageFileProcessorBase.h"
#include "io/StreamPipe.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "utils/ArrayUtils.h"
#include "utils/Enum.h"
#include "minifi-cpp/utils/Export.h"

template<typename AzureDataLakeStorageProcessor>
class AzureDataLakeStorageTestsFixture;

namespace org::apache::nifi::minifi::azure {

enum class FileExistsResolutionStrategy {
  fail,
  replace,
  ignore
};

namespace processors {

class PutAzureDataLakeStorage final : public AzureDataLakeStorageFileProcessorBase {
 public:
  EXTENSIONAPI static constexpr const char *Description = "Puts content into an Azure Data Lake Storage Gen 2";

  EXTENSIONAPI static constexpr auto ConflictResolutionStrategy
    = core::PropertyDefinitionBuilder<magic_enum::enum_count<azure::FileExistsResolutionStrategy>()>::createProperty("Conflict Resolution Strategy")
      .withDescription("Indicates what should happen when a file with the same name already exists in the output directory.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(azure::FileExistsResolutionStrategy::fail))
      .withAllowedValues(magic_enum::enum_names<azure::FileExistsResolutionStrategy>())
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureDataLakeStorageFileProcessorBase::Properties, std::to_array<core::PropertyReference>({ConflictResolutionStrategy}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Files that have been successfully written to Azure storage are transferred to this relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Files that could not be written to Azure storage for some reason are transferred to this relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  using AzureDataLakeStorageFileProcessorBase::AzureDataLakeStorageFileProcessorBase;

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  friend class ::AzureDataLakeStorageTestsFixture<PutAzureDataLakeStorage>;

  class ReadCallback {
   public:
    ReadCallback(uint64_t flow_size, storage::AzureDataLakeStorage &azure_data_lake_storage, const storage::PutAzureDataLakeStorageParameters &params, std::shared_ptr<core::logging::Logger> logger);
    int64_t operator()(const std::shared_ptr<io::InputStream> &stream);

    [[nodiscard]] storage::UploadDataLakeStorageResult getResult() const {
      return result_;
    }

   private:
    uint64_t flow_size_;
    storage::AzureDataLakeStorage &azure_data_lake_storage_;
    const storage::PutAzureDataLakeStorageParameters &params_;
    storage::UploadDataLakeStorageResult result_;
    std::shared_ptr<core::logging::Logger> logger_;
  };

  explicit PutAzureDataLakeStorage(core::ProcessorMetadata metadata, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
      : AzureDataLakeStorageFileProcessorBase(metadata, std::move(data_lake_storage_client)) {
  }

  std::optional<storage::PutAzureDataLakeStorageParameters> buildUploadParameters(core::ProcessContext &context, const core::FlowFile& flow_file);

  azure::FileExistsResolutionStrategy conflict_resolution_strategy_;
};

}  // namespace processors
}  // namespace org::apache::nifi::minifi::azure
