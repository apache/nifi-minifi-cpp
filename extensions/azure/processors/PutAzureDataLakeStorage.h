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

#include "AzureDataLakeStorageProcessor.h"

#include "utils/Enum.h"
#include "utils/Export.h"

template<typename AzureDataLakeStorageProcessor>
class AzureDataLakeStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class PutAzureDataLakeStorage final : public AzureDataLakeStorageProcessor {
 public:
  // Supported Properties
  EXTENSIONAPI static const core::Property FileName;
  EXTENSIONAPI static const core::Property ConflictResolutionStrategy;

  // Supported Relationships
  EXTENSIONAPI static const core::Relationship Failure;
  EXTENSIONAPI static const core::Relationship Success;

  SMART_ENUM(FileExistsResolutionStrategy,
    (FAIL_FLOW, "fail"),
    (REPLACE_FILE, "replace"),
    (IGNORE_REQUEST, "ignore")
  )

  explicit PutAzureDataLakeStorage(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : PutAzureDataLakeStorage(name, uuid, nullptr) {
  }

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::AzureDataLakeStorageTestsFixture<PutAzureDataLakeStorage>;

  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(uint64_t flow_size, storage::AzureDataLakeStorage& azure_data_lake_storage, const storage::PutAzureDataLakeStorageParameters& params, std::shared_ptr<core::logging::Logger> logger);
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;

    storage::UploadDataLakeStorageResult getResult() const {
      return result_;
    }

   private:
    uint64_t flow_size_;
    storage::AzureDataLakeStorage& azure_data_lake_storage_;
    const storage::PutAzureDataLakeStorageParameters& params_;
    storage::UploadDataLakeStorageResult result_;
    std::shared_ptr<core::logging::Logger> logger_;
  };

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  bool isSingleThreaded() const override {
    return true;
  }

  explicit PutAzureDataLakeStorage(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageProcessor(name, uuid, core::logging::LoggerFactory<PutAzureDataLakeStorage>::getLogger(), std::move(data_lake_storage_client)) {
  }

  std::optional<storage::PutAzureDataLakeStorageParameters> buildUploadParameters(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file);

  FileExistsResolutionStrategy conflict_resolution_strategy_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
