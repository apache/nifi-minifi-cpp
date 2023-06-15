/**
 * @file DeleteAzureDataLakeStorage.h
 * DeleteAzureDataLakeStorage class declaration
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

  explicit DeleteAzureDataLakeStorage(std::string name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : AzureDataLakeStorageFileProcessorBase(std::move(name), uuid, core::logging::LoggerFactory<DeleteAzureDataLakeStorage>::getLogger(uuid)) {
  }

  ~DeleteAzureDataLakeStorage() override = default;

  void initialize() override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::AzureDataLakeStorageTestsFixture<DeleteAzureDataLakeStorage>;

  explicit DeleteAzureDataLakeStorage(std::string name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageFileProcessorBase(std::move(name), uuid, core::logging::LoggerFactory<DeleteAzureDataLakeStorage>::getLogger(), std::move(data_lake_storage_client)) {
  }

  std::optional<storage::DeleteAzureDataLakeStorageParameters> buildDeleteParameters(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
