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

#include "AzureDataLakeStorageProcessor.h"

template<typename AzureDataLakeStorageProcessor>
class AzureDataLakeStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class DeleteAzureDataLakeStorage final : public AzureDataLakeStorageProcessor {
 public:
  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  explicit DeleteAzureDataLakeStorage(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : AzureDataLakeStorageProcessor(name, uuid, logging::LoggerFactory<DeleteAzureDataLakeStorage>::getLogger()) {
  }

  ~DeleteAzureDataLakeStorage() override = default;

  void initialize() override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::AzureDataLakeStorageTestsFixture<DeleteAzureDataLakeStorage>;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  explicit DeleteAzureDataLakeStorage(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageProcessor(name, uuid, logging::LoggerFactory<DeleteAzureDataLakeStorage>::getLogger(), std::move(data_lake_storage_client)) {
  }

  std::optional<storage::DeleteAzureDataLakeStorageParameters> buildDeleteParameters(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
