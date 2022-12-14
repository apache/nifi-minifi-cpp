/**
 * @file AzureBlobStorageProcessorBase.h
 * AzureBlobStorageProcessorBase class declaration
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

#include "core/Property.h"
#include "core/logging/Logger.h"
#include "storage/AzureBlobStorage.h"
#include "AzureStorageProcessorBase.h"
#include "storage/AzureStorageCredentials.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

class AzureBlobStorageProcessorBase : public AzureStorageProcessorBase {
 public:
  EXTENSIONAPI static const core::Property ContainerName;
  EXTENSIONAPI static const core::Property StorageAccountName;
  EXTENSIONAPI static const core::Property StorageAccountKey;
  EXTENSIONAPI static const core::Property SASToken;
  EXTENSIONAPI static const core::Property CommonStorageAccountEndpointSuffix;
  EXTENSIONAPI static const core::Property ConnectionString;
  EXTENSIONAPI static const core::Property UseManagedIdentityCredentials;
  static auto properties() {
    return utils::array_cat(AzureStorageProcessorBase::properties(), std::array{
      ContainerName,
      StorageAccountName,
      StorageAccountKey,
      SASToken,
      CommonStorageAccountEndpointSuffix,
      ConnectionString,
      UseManagedIdentityCredentials
    });
  }

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 protected:
  explicit AzureBlobStorageProcessorBase(
    std::string name,
    const minifi::utils::Identifier& uuid,
    const std::shared_ptr<core::logging::Logger>& logger,
    std::unique_ptr<storage::BlobStorageClient> blob_storage_client)
    : AzureStorageProcessorBase(std::move(name), uuid, logger),
      azure_blob_storage_(std::move(blob_storage_client)) {
  }

  storage::AzureStorageCredentials getAzureCredentialsFromProperties(
    core::ProcessContext &context, const std::shared_ptr<core::FlowFile> &flow_file) const;
  std::optional<storage::AzureStorageCredentials> getCredentials(
    core::ProcessContext &context, const std::shared_ptr<core::FlowFile> &flow_file) const;
  bool setCommonStorageParameters(
    storage::AzureBlobStorageParameters& params,
    core::ProcessContext &context,
    const std::shared_ptr<core::FlowFile> &flow_file);

  storage::AzureBlobStorage azure_blob_storage_;
  bool use_managed_identity_credentials_ = false;
};

}  // namespace org::apache::nifi::minifi::azure::processors
