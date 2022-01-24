/**
 * @file FetchAzureBlobStorage.cpp
 * FetchAzureBlobStorage class implementation
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

#include "FetchAzureBlobStorage.h"

#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

const core::Property FetchAzureBlobStorage::RangeStart(
  core::PropertyBuilder::createProperty("Range Start")
    ->withDescription("The byte position at which to start reading from the blob. An empty value or a value of zero will start reading at the beginning of the blob.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property FetchAzureBlobStorage::RangeLength(
  core::PropertyBuilder::createProperty("Range Length")
    ->withDescription("The number of bytes to download from the blob, starting from the Range Start. "
                      "An empty value or a value that extends beyond the end of the blob will read to the end of the blob.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Relationship FetchAzureBlobStorage::Success("success", "All successfully processed FlowFiles are routed to this relationship");
const core::Relationship FetchAzureBlobStorage::Failure("failure", "Unsuccessful operations will be transferred to the failure relationship");

namespace {
class WriteCallback : public OutputStreamCallback {
 public:
  WriteCallback(storage::AzureBlobStorage& azure_blob_storage, const storage::FetchAzureBlobStorageParameters& params, std::shared_ptr<core::logging::Logger> logger)
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
  std::shared_ptr<core::logging::Logger> logger_;
};
}  // namespace

void FetchAzureBlobStorage::initialize() {
  setSupportedProperties({
    AzureStorageCredentialsService,
    ContainerName,
    StorageAccountName,
    StorageAccountKey,
    SASToken,
    CommonStorageAccountEndpointSuffix,
    ConnectionString,
    Blob,
    UseManagedIdentityCredentials,
    RangeStart,
    RangeLength
  });
  setSupportedRelationships({
    Success,
    Failure
  });
}

std::optional<storage::FetchAzureBlobStorageParameters> FetchAzureBlobStorage::buildFetchAzureBlobStorageParameters(
    core::ProcessContext &context, const std::shared_ptr<core::FlowFile> &flow_file) {
  storage::FetchAzureBlobStorageParameters params;
  if (!setCommonStorageParameters(params, context, flow_file)) {
    return std::nullopt;
  }

  std::string value;
  if (context.getProperty(RangeStart, value, flow_file)) {
    params.range_start = std::stoull(value);
    logger_->log_debug("Range Start property set to %llu", *params.range_start);
  }

  if (context.getProperty(RangeLength, value, flow_file)) {
    params.range_length = std::stoull(value);
    logger_->log_debug("Range Length property set to %llu", *params.range_length);
  }

  return params;
}

void FetchAzureBlobStorage::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  gsl_Expects(context && session);
  logger_->log_trace("FetchAzureBlobStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  const auto params = buildFetchAzureBlobStorageParameters(*context, flow_file);
  if (!params) {
    session->transfer(flow_file, Failure);
    return;
  }

  auto fetched_flow_file = session->create(flow_file);
  WriteCallback callback(azure_blob_storage_, *params, logger_);
  session->write(fetched_flow_file, &callback);

  if (callback.getResult() == std::nullopt) {
    logger_->log_error("Failed to fetch blob '%s' from Azure Blob storage", params->blob_name);
    session->transfer(flow_file, Failure);
    session->remove(fetched_flow_file);
  } else {
    logger_->log_debug("Successfully fetched blob '%s' from container '%s' on Azure Blob storage", params->blob_name, params->container_name);
    session->transfer(fetched_flow_file, Success);
    session->remove(flow_file);
  }
}

REGISTER_RESOURCE(FetchAzureBlobStorage, "Retrieves contents of an Azure Storage Blob, writing the contents to the content of the FlowFile");

}  // namespace org::apache::nifi::minifi::azure::processors
