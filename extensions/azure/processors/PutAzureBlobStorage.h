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

#include "core/Property.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "storage/AzureBlobStorage.h"
#include "AzureStorageProcessorBase.h"
#include "storage/AzureStorageCredentials.h"

class PutAzureBlobStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class PutAzureBlobStorage final : public AzureStorageProcessorBase {
 public:
  // Supported Properties
  static const core::Property ContainerName;
  static const core::Property StorageAccountName;
  static const core::Property StorageAccountKey;
  static const core::Property SASToken;
  static const core::Property CommonStorageAccountEndpointSuffix;
  static const core::Property ConnectionString;
  static const core::Property Blob;
  static const core::Property CreateContainer;
  static const core::Property UseManagedIdentityCredentials;

  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  explicit PutAzureBlobStorage(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : PutAzureBlobStorage(name, uuid, nullptr) {
  }

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(uint64_t flow_size, storage::AzureBlobStorage& azure_blob_storage, const storage::PutAzureBlobStorageParameters& params)
      : flow_size_(flow_size)
      , azure_blob_storage_(azure_blob_storage)
      , params_(params) {
    }

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      std::vector<uint8_t> buffer;
      size_t read_ret = stream->read(buffer, flow_size_);
      if (io::isError(read_ret) || read_ret != flow_size_) {
        return -1;
      }

      result_ = azure_blob_storage_.uploadBlob(params_, gsl::make_span(buffer.data(), flow_size_));
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
  friend class ::PutAzureBlobStorageTestsFixture;

  explicit PutAzureBlobStorage(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureStorageProcessorBase(name, uuid, logging::LoggerFactory<PutAzureBlobStorage>::getLogger())
    , azure_blob_storage_(std::move(blob_storage_client)) {
  }

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  bool isSingleThreaded() const override {
    return true;
  }

  storage::AzureStorageCredentials getAzureCredentialsFromProperties(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) const;
  std::optional<storage::AzureStorageCredentials> getCredentials(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) const;
  std::optional<storage::PutAzureBlobStorageParameters> buildAzureBlobStorageParameters(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file);

  storage::AzureBlobStorage azure_blob_storage_;
  bool create_container_ = false;
  bool use_managed_identity_credentials_ = false;
};

}  // namespace org::apache::nifi::minifi::azure::processors
