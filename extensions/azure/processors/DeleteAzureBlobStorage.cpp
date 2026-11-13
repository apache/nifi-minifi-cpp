// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "DeleteAzureBlobStorage.h"

#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

void DeleteAzureBlobStorage::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void DeleteAzureBlobStorage::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  AzureBlobStorageProcessorBase::onSchedule(context, session_factory);
  optional_deletion_ = utils::parseEnumProperty<storage::OptionalDeletion>(context, DeleteSnapshotsOption);
}

std::optional<storage::DeleteAzureBlobStorageParameters> DeleteAzureBlobStorage::buildDeleteAzureBlobStorageParameters(
    core::ProcessContext& context, const core::FlowFile& flow_file) {
  storage::DeleteAzureBlobStorageParameters params;
  if (!setBlobOperationParameters(params, context, flow_file)) {
    return std::nullopt;
  }
  params.optional_deletion = optional_deletion_;
  return params;
}

void DeleteAzureBlobStorage::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("DeleteAzureBlobStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session.get();
  if (!flow_file) {
    return;
  }

  auto params = buildDeleteAzureBlobStorageParameters(context, *flow_file);
  if (!params) {
    session.transfer(flow_file, Failure);
    return;
  }

  auto result = azure_blob_storage_.deleteBlob(*params);
  if (result) {
    logger_->log_debug("Successfully deleted blob '{}' from Azure Storage container '{}'", params->blob_name, params->container_name);
    session.transfer(flow_file, Success);
  } else {
    logger_->log_error("Failed to delete blob '{}' from Azure Storage container '{}'", params->blob_name, params->container_name);
    session.transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(DeleteAzureBlobStorage, Processor);

}  // namespace org::apache::nifi::minifi::azure::processors
