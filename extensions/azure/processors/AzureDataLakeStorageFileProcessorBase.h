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

#include "AzureDataLakeStorageProcessorBase.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

class AzureDataLakeStorageFileProcessorBase : public AzureDataLakeStorageProcessorBase {
 public:
  EXTENSIONAPI static constexpr auto FileName = core::PropertyDefinitionBuilder<>::createProperty("File Name")
      .withDescription("The filename in Azure Storage. If left empty the filename attribute will be used by default.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureDataLakeStorageProcessorBase::Properties, std::to_array<core::PropertyReference>({FileName}));

  using AzureDataLakeStorageProcessorBase::AzureDataLakeStorageProcessorBase;

  ~AzureDataLakeStorageFileProcessorBase() override = default;

 protected:
  explicit AzureDataLakeStorageFileProcessorBase(core::ProcessorMetadata metadata, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageProcessorBase(metadata, std::move(data_lake_storage_client)) {
  }

  bool setFileOperationCommonParameters(
    storage::AzureDataLakeStorageFileOperationParameters& params,
    core::ProcessContext& context,
    const core::FlowFile& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
