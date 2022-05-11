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
#include "AzureBlobStorageSingleBlobProcessorBase.h"
#include "core/logging/LoggerConfiguration.h"

template<typename T>
class AzureBlobStorageTestsFixture;

namespace org::apache::nifi::minifi::azure::processors {

class FetchAzureBlobStorage final : public AzureBlobStorageSingleBlobProcessorBase {
 public:
  EXTENSIONAPI static const core::Property RangeStart;
  EXTENSIONAPI static const core::Property RangeLength;

  static const core::Relationship Failure;
  static const core::Relationship Success;

  explicit FetchAzureBlobStorage(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : FetchAzureBlobStorage(name, uuid, nullptr) {
  }

  void initialize() override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::AzureBlobStorageTestsFixture<FetchAzureBlobStorage>;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  explicit FetchAzureBlobStorage(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureBlobStorageSingleBlobProcessorBase(name, uuid, core::logging::LoggerFactory<FetchAzureBlobStorage>::getLogger(), std::move(blob_storage_client)) {
  }

  std::optional<storage::FetchAzureBlobStorageParameters> buildFetchAzureBlobStorageParameters(
    core::ProcessContext &context, const std::shared_ptr<core::FlowFile> &flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
