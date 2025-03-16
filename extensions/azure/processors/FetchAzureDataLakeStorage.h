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

#include "core/PropertyDefinition.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "io/StreamPipe.h"
#include "utils/ArrayUtils.h"
#include "AzureDataLakeStorageFileProcessorBase.h"

template<typename AzureDataLakeStorageProcessor>
class AzureDataLakeStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class FetchAzureDataLakeStorage final : public AzureDataLakeStorageFileProcessorBase {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Fetch the provided file from Azure Data Lake Storage Gen 2";

  EXTENSIONAPI static constexpr auto RangeStart = core::PropertyDefinitionBuilder<>::createProperty("Range Start")
      .withDescription("The byte position at which to start reading from the object. An empty value or a value of zero will start reading at the beginning of the object.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto RangeLength = core::PropertyDefinitionBuilder<>::createProperty("Range Length")
      .withDescription("The number of bytes to download from the object, starting from the Range Start. "
                        "An empty value or a value that extends beyond the end of the object will read to the end of the object.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto NumberOfRetries = core::PropertyDefinitionBuilder<>::createProperty("Number of Retries")
      .withDescription("The number of automatic retries to perform if the download fails.")
      .withValidator(core::StandardPropertyTypes::UNSIGNED_INTEGER_VALIDATOR)
      .withDefaultValue("0")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureDataLakeStorageFileProcessorBase::Properties, std::to_array<core::PropertyReference>({
      RangeStart,
      RangeLength,
      NumberOfRetries
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Files that have been successfully fetched from Azure storage are transferred to this relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "In case of fetch failure flowfiles are transferred to this relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit FetchAzureDataLakeStorage(std::string_view name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : AzureDataLakeStorageFileProcessorBase(name, uuid, core::logging::LoggerFactory<FetchAzureDataLakeStorage>::getLogger(uuid)) {
  }

  ~FetchAzureDataLakeStorage() override = default;

  void initialize() override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  friend class ::AzureDataLakeStorageTestsFixture<FetchAzureDataLakeStorage>;

  explicit FetchAzureDataLakeStorage(std::string_view name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageFileProcessorBase(name, uuid, core::logging::LoggerFactory<FetchAzureDataLakeStorage>::getLogger(), std::move(data_lake_storage_client)) {
  }

  std::optional<storage::FetchAzureDataLakeStorageParameters> buildFetchParameters(core::ProcessContext& context, const core::FlowFile& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
