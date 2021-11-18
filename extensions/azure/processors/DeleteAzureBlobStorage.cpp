/**
 * @file DeleteAzureBlobStorage.cpp
 * DeleteAzureBlobStorage class implementation
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

#include "DeleteAzureBlobStorage.h"

#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

const core::Property DeleteAzureBlobStorage::DeleteSnapshotsOption(
  core::PropertyBuilder::createProperty("Delete Snapshots Option")
    ->withDescription("Specifies the snapshot deletion options to be used when deleting a blob. None: Deletes the blob only. Include Snapshots: Delete the blob and its snapshots. "
                      "Delete Snapshots Only: Delete only the blob's snapshots.")
    ->isRequired(true)
    ->withDefaultValue<std::string>(toString(storage::OptionalDeletion::NONE))
    ->withAllowableValues<std::string>(storage::OptionalDeletion::values())
    ->build());

const core::Relationship DeleteAzureBlobStorage::Success("success", "All successfully processed FlowFiles are routed to this relationship");
const core::Relationship DeleteAzureBlobStorage::Failure("failure", "Unsuccessful operations will be transferred to the failure relationship");

void DeleteAzureBlobStorage::initialize() {
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
    DeleteSnapshotsOption,
    UseManagedIdentityCredentials
  });
  // Set the supported relationships
  setSupportedRelationships({
    Success,
    Failure
  });
}

void DeleteAzureBlobStorage::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& session_factory) {
  gsl_Expects(context && session_factory);
  AzureBlobStorageProcessorBase::onSchedule(context, session_factory);
  optional_deletion_ = storage::OptionalDeletion::parse(
    utils::parsePropertyWithAllowableValuesOrThrow(*context, DeleteSnapshotsOption.getName(), storage::OptionalDeletion::values()).c_str());
}

std::optional<storage::DeleteAzureBlobStorageParameters> DeleteAzureBlobStorage::buildDeleteAzureBlobStorageParameters(
    core::ProcessContext &context, const std::shared_ptr<core::FlowFile> &flow_file) {
  storage::DeleteAzureBlobStorageParameters params;
  if (!setCommonStorageParameters(params, context, flow_file)) {
    return std::nullopt;
  }
  params.optional_deletion = optional_deletion_;
  return params;
}

void DeleteAzureBlobStorage::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  gsl_Expects(context && session);
  logger_->log_trace("DeleteAzureBlobStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    return;
  }

  auto params = buildDeleteAzureBlobStorageParameters(*context, flow_file);
  if (!params) {
    session->transfer(flow_file, Failure);
    return;
  }

  auto result = azure_blob_storage_.deleteBlob(*params);
  if (result) {
    logger_->log_debug("Successfully deleted blob '%s' from Azure Storage container '%s'", params->blob_name, params->container_name);
    session->transfer(flow_file, Success);
  } else {
    logger_->log_error("Failed to delete blob '%s' from Azure Storage container '%s'", params->blob_name, params->container_name);
    session->transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(DeleteAzureBlobStorage, "Deletes the provided blob from Azure Storage");

}  // namespace org::apache::nifi::minifi::azure::processors
