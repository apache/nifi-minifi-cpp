/**
 * @file AzureDataLakeStorageProcessorBase.h
 * AzureDataLakeStorageProcessorBase class declaration
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

#include <utility>
#include <string>
#include <memory>
#include <optional>

#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "storage/AzureDataLakeStorage.h"
#include "utils/ArrayUtils.h"
#include "AzureStorageProcessorBase.h"

namespace org::apache::nifi::minifi::azure::processors {

class AzureDataLakeStorageProcessorBase : public AzureStorageProcessorBase {
 public:
  EXTENSIONAPI static constexpr auto FilesystemName = core::PropertyDefinitionBuilder<>::createProperty("Filesystem Name")
      .withDescription("Name of the Azure Storage File System. It is assumed to be already existing.")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto DirectoryName = core::PropertyDefinitionBuilder<>::createProperty("Directory Name")
      .withDescription("Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. "
          "If left empty it designates the root directory. The directory will be created if not already existing.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureStorageProcessorBase::Properties, std::to_array<core::PropertyReference>({
      FilesystemName,
      DirectoryName
  }));

  using AzureStorageProcessorBase::AzureStorageProcessorBase;

  ~AzureDataLakeStorageProcessorBase() override = default;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  explicit AzureDataLakeStorageProcessorBase(core::ProcessorMetadata metadata, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureStorageProcessorBase(metadata),
      azure_data_lake_storage_(std::move(data_lake_storage_client)) {
  }

  bool setCommonParameters(storage::AzureDataLakeStorageParameters& params, core::ProcessContext& context, const core::FlowFile* const flow_file);

  storage::AzureStorageCredentials credentials_;
  storage::AzureDataLakeStorage azure_data_lake_storage_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
