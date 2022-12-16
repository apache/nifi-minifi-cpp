/**
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

#include "io/StreamPipe.h"
#include "utils/ArrayUtils.h"
#include "AzureDataLakeStorageFileProcessorBase.h"

template<typename AzureDataLakeStorageProcessor>
class AzureDataLakeStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class FetchAzureDataLakeStorage final : public AzureDataLakeStorageFileProcessorBase {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Fetch the provided file from Azure Data Lake Storage Gen 2";

  EXTENSIONAPI static const core::Property RangeStart;
  EXTENSIONAPI static const core::Property RangeLength;
  EXTENSIONAPI static const core::Property NumberOfRetries;
  static auto properties() {
    return utils::array_cat(AzureDataLakeStorageFileProcessorBase::properties(), std::array{
      RangeStart,
      RangeLength,
      NumberOfRetries
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit FetchAzureDataLakeStorage(std::string name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : AzureDataLakeStorageFileProcessorBase(std::move(name), uuid, core::logging::LoggerFactory<FetchAzureDataLakeStorage>::getLogger(uuid)) {
  }

  ~FetchAzureDataLakeStorage() override = default;

  void initialize() override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::AzureDataLakeStorageTestsFixture<FetchAzureDataLakeStorage>;

  explicit FetchAzureDataLakeStorage(std::string name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageFileProcessorBase(std::move(name), uuid, core::logging::LoggerFactory<FetchAzureDataLakeStorage>::getLogger(), std::move(data_lake_storage_client)) {
  }

  std::optional<storage::FetchAzureDataLakeStorageParameters> buildFetchParameters(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
