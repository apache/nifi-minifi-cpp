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

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::azure::processors {

const core::Property PutAzureBlobStorage::CreateContainer(
  core::PropertyBuilder::createProperty("Create Container")
    ->withDescription("Specifies whether to check if the container exists and to automatically create it if it does not. "
                      "Permission to list containers is required. If false, this check is not made, but the Put operation will "
                      "fail if the container does not exist.")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->build());

const core::Relationship PutAzureBlobStorage::Success("success", "All successfully processed FlowFiles are routed to this relationship");
const core::Relationship PutAzureBlobStorage::Failure("failure", "Unsuccessful operations will be transferred to the failure relationship");

void PutAzureBlobStorage::initialize() {
  // Set the supported properties
  setSupportedProperties({
    AzureStorageCredentialsService,
    ContainerName,
    StorageAccountName,
    StorageAccountKey,
    SASToken,
    CommonStorageAccountEndpointSuffix,
    ConnectionString,
    Blob,
    CreateContainer,
    UseManagedIdentityCredentials
  });
  // Set the supported relationships
  setSupportedRelationships({
    Success,
    Failure
  });
}


void PutAzureBlobStorage::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& session_factory) {
  gsl_Expects(context && session_factory);
  AzureBlobStorageProcessorBase::onSchedule(context, session_factory);
  context->getProperty(CreateContainer.getName(), create_container_);
}

std::optional<storage::PutAzureBlobStorageParameters> PutAzureBlobStorage::buildPutAzureBlobStorageParameters(
    core::ProcessContext &context,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  storage::PutAzureBlobStorageParameters params;
  if (!setCommonStorageParameters(params, context, flow_file)) {
    return std::nullopt;
  }

  return params;
}

void PutAzureBlobStorage::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  gsl_Expects(context && session);
  logger_->log_trace("PutAzureBlobStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    return;
  }

  auto params = buildPutAzureBlobStorageParameters(*context, flow_file);
  if (!params) {
    session->transfer(flow_file, Failure);
    return;
  }

  if (create_container_) {
    auto result = azure_blob_storage_.createContainerIfNotExists(*params);
    if (!result) {
      session->transfer(flow_file, Failure);
      return;
    }
  }
  PutAzureBlobStorage::ReadCallback callback(flow_file->getSize(), azure_blob_storage_, *params);
  session->read(flow_file, std::ref(callback));
  const std::optional<storage::UploadBlobResult> upload_result = callback.getResult();

  if (!upload_result) {
    logger_->log_error("Failed to upload blob '%s' to Azure Storage container '%s'", params->blob_name, params->container_name);
    session->transfer(flow_file, Failure);
    return;
  }

  session->putAttribute(flow_file, "azure.container", params->container_name);
  session->putAttribute(flow_file, "azure.blobname", params->blob_name);
  session->putAttribute(flow_file, "azure.primaryUri", upload_result->primary_uri);
  session->putAttribute(flow_file, "azure.etag", upload_result->etag);
  session->putAttribute(flow_file, "azure.length", std::to_string(flow_file->getSize()));
  session->putAttribute(flow_file, "azure.timestamp", upload_result->timestamp);
  logger_->log_debug("Successfully uploaded blob '%s' to Azure Storage container '%s'", params->blob_name, params->container_name);
  session->transfer(flow_file, Success);
}

REGISTER_RESOURCE(PutAzureBlobStorage, "Puts content into an Azure Storage Blob");

}  // namespace org::apache::nifi::minifi::azure::processors
