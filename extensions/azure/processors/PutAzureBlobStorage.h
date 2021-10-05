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
#include "core/logging/LoggerConfiguration.h"
#include "AzureBlobStorageProcessorBase.h"

template<typename T>
class AzureBlobStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class PutAzureBlobStorage final : public AzureBlobStorageProcessorBase {
 public:
  // Supported Properties
  static const core::Property CreateContainer;

  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  explicit PutAzureBlobStorage(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : PutAzureBlobStorage(name, uuid, nullptr) {
  }

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;
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
  friend class ::AzureBlobStorageTestsFixture<PutAzureBlobStorage>;

  explicit PutAzureBlobStorage(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureBlobStorageProcessorBase(name, uuid, core::logging::LoggerFactory<PutAzureBlobStorage>::getLogger(), std::move(blob_storage_client)) {
  }

  std::optional<storage::PutAzureBlobStorageParameters> buildPutAzureBlobStorageParameters(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file);

  bool create_container_ = false;
};

}  // namespace org::apache::nifi::minifi::azure::processors
