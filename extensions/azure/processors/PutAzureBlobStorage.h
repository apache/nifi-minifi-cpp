/**
 * @file PutAzureBlobStorage.h
 * PutAzureBlobStorage class declaration
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

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/ProcessContext.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyType.h"
#include "core/Property.h"
#include "core/logging/LoggerFactory.h"
#include "AzureBlobStorageSingleBlobProcessorBase.h"
#include "io/StreamPipe.h"
#include "utils/ArrayUtils.h"

template<typename T>
class AzureBlobStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class PutAzureBlobStorage final : public AzureBlobStorageSingleBlobProcessorBase {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Puts content into an Azure Storage Blob";

  EXTENSIONAPI static constexpr auto CreateContainer = core::PropertyDefinitionBuilder<>::createProperty("Create Container")
    .withDescription("Specifies whether to check if the container exists and to automatically create it if it does not. "
        "Permission to list containers is required. If false, this check is not made, but the Put operation will "
        "fail if the container does not exist.")
    .isRequired(true)
    .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
    .withDefaultValue("false")
    .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(AzureBlobStorageSingleBlobProcessorBase::Properties, std::to_array<core::PropertyReference>({CreateContainer}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All successfully processed FlowFiles are routed to this relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Unsuccessful operations will be transferred to the failure relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutAzureBlobStorage(std::string_view name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : PutAzureBlobStorage(name, uuid, nullptr) {
  }

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  class ReadCallback {
   public:
    ReadCallback(uint64_t flow_size, storage::AzureBlobStorage& azure_blob_storage, const storage::PutAzureBlobStorageParameters& params)
      : flow_size_(flow_size)
      , azure_blob_storage_(azure_blob_storage)
      , params_(params) {
    }

    int64_t operator()(const std::shared_ptr<io::InputStream>& stream) {
      std::vector<std::byte> buffer;
      buffer.resize(flow_size_);
      size_t read_ret = stream->read(buffer);
      if (io::isError(read_ret) || read_ret != flow_size_) {
        return -1;
      }

      result_ = azure_blob_storage_.uploadBlob(params_, buffer);
      return read_ret;
    }

    std::optional<storage::UploadBlobResult> getResult() const {
      return result_;
    }

   private:
    uint64_t flow_size_;
    storage::AzureBlobStorage &azure_blob_storage_;
    const storage::PutAzureBlobStorageParameters& params_;
    std::optional<storage::UploadBlobResult> result_ = std::nullopt;
  };

 private:
  friend class ::AzureBlobStorageTestsFixture<PutAzureBlobStorage>;

  explicit PutAzureBlobStorage(std::string_view name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureBlobStorageSingleBlobProcessorBase(name, uuid, core::logging::LoggerFactory<PutAzureBlobStorage>::getLogger(), std::move(blob_storage_client)) {
  }

  std::optional<storage::PutAzureBlobStorageParameters> buildPutAzureBlobStorageParameters(core::ProcessContext &context, const core::FlowFile& flow_file);

  bool create_container_ = false;
};

}  // namespace org::apache::nifi::minifi::azure::processors
