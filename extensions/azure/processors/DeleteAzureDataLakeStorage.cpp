// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "DeleteAzureDataLakeStorage.h"

#include "utils/ProcessorConfigUtils.h"
#include "minifi-cpp/utils/gsl.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::azure::processors {

void DeleteAzureDataLakeStorage::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

std::optional<storage::DeleteAzureDataLakeStorageParameters> DeleteAzureDataLakeStorage::buildDeleteParameters(
    core::ProcessContext& context, const core::FlowFile& flow_file) {
  storage::DeleteAzureDataLakeStorageParameters params;
  if (!setFileOperationCommonParameters(params, context, flow_file)) {
    return std::nullopt;
  }

  return params;
}

void DeleteAzureDataLakeStorage::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("DeleteAzureDataLakeStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  const auto params = buildDeleteParameters(context, *flow_file);
  if (!params) {
    session.transfer(flow_file, Failure);
    return;
  }

  auto result = azure_data_lake_storage_.deleteFile(*params);
  if (!result) {
    logger_->log_error("Failed to delete file '{}' to Azure Data Lake storage", params->filename);
    session.transfer(flow_file, Failure);
  } else {
    logger_->log_debug("Successfully deleted file '{}' of filesystem '{}' on Azure Data Lake storage", params->filename, params->file_system_name);
    session.transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(DeleteAzureDataLakeStorage, Processor);

}  // namespace org::apache::nifi::minifi::azure::processors
