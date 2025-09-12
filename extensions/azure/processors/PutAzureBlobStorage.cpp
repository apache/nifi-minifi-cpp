/**
 * @file PutAzureBlobStorage.cpp
 * PutAzureBlobStorage class implementation
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

#include "PutAzureBlobStorage.h"

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"


namespace org::apache::nifi::minifi::azure::processors {

void PutAzureBlobStorage::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}


void PutAzureBlobStorage::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  AzureBlobStorageProcessorBase::onSchedule(context, session_factory);
  create_container_ = minifi::utils::parseBoolProperty(context, CreateContainer);
}

std::optional<storage::PutAzureBlobStorageParameters> PutAzureBlobStorage::buildPutAzureBlobStorageParameters(
    core::ProcessContext &context,
    const core::FlowFile& flow_file) {
  storage::PutAzureBlobStorageParameters params;
  if (!setBlobOperationParameters(params, context, flow_file)) {
    return std::nullopt;
  }

  return params;
}

void PutAzureBlobStorage::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("PutAzureBlobStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session.get();
  if (!flow_file) {
    return;
  }

  auto params = buildPutAzureBlobStorageParameters(context, *flow_file);
  if (!params) {
    session.transfer(flow_file, Failure);
    return;
  }

  if (create_container_) {
    auto result = azure_blob_storage_.createContainerIfNotExists(*params);
    if (!result) {
      session.transfer(flow_file, Failure);
      return;
    }
  }
  PutAzureBlobStorage::ReadCallback callback(flow_file->getSize(), azure_blob_storage_, *params);
  session.read(flow_file, std::ref(callback));
  const std::optional<storage::UploadBlobResult> upload_result = callback.getResult();

  if (!upload_result) {
    logger_->log_error("Failed to upload blob '{}' to Azure Storage container '{}'", params->blob_name, params->container_name);
    session.transfer(flow_file, Failure);
    return;
  }

  session.putAttribute(*flow_file, "azure.container", params->container_name);
  session.putAttribute(*flow_file, "azure.blobname", params->blob_name);
  session.putAttribute(*flow_file, "azure.primaryUri", upload_result->primary_uri);
  session.putAttribute(*flow_file, "azure.etag", upload_result->etag);
  session.putAttribute(*flow_file, "azure.length", std::to_string(flow_file->getSize()));
  session.putAttribute(*flow_file, "azure.timestamp", upload_result->timestamp);
  logger_->log_debug("Successfully uploaded blob '{}' to Azure Storage container '{}'", params->blob_name, params->container_name);
  session.transfer(flow_file, Success);
}

REGISTER_RESOURCE(PutAzureBlobStorage, Processor);

}  // namespace org::apache::nifi::minifi::azure::processors
