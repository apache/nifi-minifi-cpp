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

class ListAzureDataLakeStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class ListAzureDataLakeStorage final : public AzureDataLakeStorageProcessorBase {
 public:
  static const core::Property RecurseSubdirectories;
  static const core::Property FileFilter;
  static const core::Property PathFilter;
  static const core::Property ListingStrategy;

  // Supported Relationships
  static const core::Relationship Success;

  explicit ListAzureDataLakeStorage(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : AzureDataLakeStorageProcessorBase(name, uuid, logging::LoggerFactory<ListAzureDataLakeStorage>::getLogger()) {
  }

  ~ListAzureDataLakeStorage() override = default;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::ListAzureDataLakeStorageTestsFixture;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

  explicit ListAzureDataLakeStorage(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageProcessorBase(name, uuid, logging::LoggerFactory<ListAzureDataLakeStorage>::getLogger(), std::move(data_lake_storage_client)) {
  }

  std::optional<storage::ListAzureDataLakeStorageParameters> buildListParameters(const std::shared_ptr<core::ProcessContext>& context);

  bool recurse_subdirectories_ = true;
  storage::EntityTracking tracking_strategy_ = storage::EntityTracking::TIMESTAMPS;
  storage::ListAzureDataLakeStorageParameters list_parameters_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
