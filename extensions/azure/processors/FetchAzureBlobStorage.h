/**
 * @file FetchAzureBlobStorage.h
 * FetchAzureBlobStorage class declaration
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

#include "core/Property.h"
#include "AzureBlobStorageProcessorBase.h"
#include "core/logging/LoggerConfiguration.h"

template<typename T>
class AzureBlobStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class FetchAzureBlobStorage final : public AzureBlobStorageProcessorBase {
 public:
  // Supported Properties
  EXTENSIONAPI static const core::Property RangeStart;
  EXTENSIONAPI static const core::Property RangeLength;

  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  explicit FetchAzureBlobStorage(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : FetchAzureBlobStorage(name, uuid, nullptr) {
  }

  void initialize() override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::AzureBlobStorageTestsFixture<FetchAzureBlobStorage>;

  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(storage::AzureBlobStorage& azure_blob_storage, const storage::FetchAzureBlobStorageParameters& params, std::shared_ptr<logging::Logger> logger)
      : azure_blob_storage_(azure_blob_storage),
        params_(params),
        logger_(std::move(logger)) {
    }

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      result_size_ = azure_blob_storage_.fetchBlob(params_, *stream);
      if (!result_size_) {
        return 0;
      }

      return gsl::narrow<int64_t>(*result_size_);
    }

    auto getResult() const {
      return result_size_;
    }

   private:
    storage::AzureBlobStorage& azure_blob_storage_;
    const storage::FetchAzureBlobStorageParameters& params_;
    std::optional<uint64_t> result_size_ = std::nullopt;
    std::shared_ptr<logging::Logger> logger_;
  };

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  explicit FetchAzureBlobStorage(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureBlobStorageProcessorBase(name, uuid, logging::LoggerFactory<FetchAzureBlobStorage>::getLogger(), std::move(blob_storage_client)) {
  }

  std::optional<storage::FetchAzureBlobStorageParameters> buildFetchAzureBlobStorageParameters(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file);

  storage::OptionalDeletion optional_deletion_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
