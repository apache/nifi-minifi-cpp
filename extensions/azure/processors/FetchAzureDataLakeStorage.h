/**
 * @file FetchAzureDataLakeStorage.h
 * FetchAzureDataLakeStorage class declaration
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

#include "AzureDataLakeStorageSingleFileProcessor.h"

template<typename AzureDataLakeStorageProcessor>
class AzureDataLakeStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class FetchAzureDataLakeStorage final : public AzureDataLakeStorageSingleFileProcessor {
 public:
  // Supported Properties
  EXTENSIONAPI static const core::Property RangeStart;
  EXTENSIONAPI static const core::Property RangeLength;
  EXTENSIONAPI static const core::Property NumberOfRetries;

  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  explicit FetchAzureDataLakeStorage(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : AzureDataLakeStorageSingleFileProcessor(name, uuid, core::logging::LoggerFactory<FetchAzureDataLakeStorage>::getLogger()) {
  }

  ~FetchAzureDataLakeStorage() override = default;

  void initialize() override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::AzureDataLakeStorageTestsFixture<FetchAzureDataLakeStorage>;

  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(storage::AzureDataLakeStorage& azure_data_lake_storage, const storage::FetchAzureDataLakeStorageParameters& params, std::shared_ptr<core::logging::Logger> logger)
      : azure_data_lake_storage_(azure_data_lake_storage),
        params_(params),
        logger_(std::move(logger)) {
    }

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      result_size_ = azure_data_lake_storage_.fetchFile(params_, *stream);
      if (!result_size_) {
        return 0;
      }

      return gsl::narrow<int64_t>(*result_size_);
    }

    auto getResult() const {
      return result_size_;
    }

   private:
    storage::AzureDataLakeStorage& azure_data_lake_storage_;
    const storage::FetchAzureDataLakeStorageParameters& params_;
    std::optional<uint64_t> result_size_ = std::nullopt;
    std::shared_ptr<core::logging::Logger> logger_;
  };

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  bool isSingleThreaded() const override {
    return true;
  }

  explicit FetchAzureDataLakeStorage(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageSingleFileProcessor(name, uuid, core::logging::LoggerFactory<FetchAzureDataLakeStorage>::getLogger(), std::move(data_lake_storage_client)) {
  }

  std::optional<storage::FetchAzureDataLakeStorageParameters> buildFetchParameters(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
