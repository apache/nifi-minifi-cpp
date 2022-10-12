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
#include "utils/ArrayUtils.h"

class ListAzureDataLakeStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class ListAzureDataLakeStorage final : public AzureDataLakeStorageProcessorBase {
 public:
  SMART_ENUM(EntityTracking,
    (NONE, "none"),
    (TIMESTAMPS, "timestamps")
  )

  EXTENSIONAPI static constexpr const char* Description = "Lists directory in an Azure Data Lake Storage Gen 2 filesystem";

  EXTENSIONAPI static const core::Property RecurseSubdirectories;
  EXTENSIONAPI static const core::Property FileFilter;
  EXTENSIONAPI static const core::Property PathFilter;
  EXTENSIONAPI static const core::Property ListingStrategy;
  static auto properties() {
    return utils::array_cat(AzureDataLakeStorageProcessorBase::properties(), std::array{
      RecurseSubdirectories,
      FileFilter,
      PathFilter,
      ListingStrategy
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit ListAzureDataLakeStorage(std::string name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : AzureDataLakeStorageProcessorBase(std::move(name), uuid, core::logging::LoggerFactory<ListAzureDataLakeStorage>::getLogger()) {
  }

  ~ListAzureDataLakeStorage() override = default;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::ListAzureDataLakeStorageTestsFixture;

  explicit ListAzureDataLakeStorage(std::string name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageProcessorBase(std::move(name), uuid, core::logging::LoggerFactory<ListAzureDataLakeStorage>::getLogger(), std::move(data_lake_storage_client)) {
  }

  std::optional<storage::ListAzureDataLakeStorageParameters> buildListParameters(core::ProcessContext& context);

  EntityTracking tracking_strategy_ = EntityTracking::TIMESTAMPS;
  storage::ListAzureDataLakeStorageParameters list_parameters_;
  std::unique_ptr<minifi::utils::ListingStateManager> state_manager_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
