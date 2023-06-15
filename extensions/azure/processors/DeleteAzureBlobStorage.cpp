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

#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

void DeleteAzureBlobStorage::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void DeleteAzureBlobStorage::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& session_factory) {
  gsl_Expects(context && session_factory);
  AzureBlobStorageProcessorBase::onSchedule(context, session_factory);
  optional_deletion_ = utils::parseEnumProperty<storage::OptionalDeletion>(*context, DeleteSnapshotsOption);
}

std::optional<storage::DeleteAzureBlobStorageParameters> DeleteAzureBlobStorage::buildDeleteAzureBlobStorageParameters(
    core::ProcessContext &context, const std::shared_ptr<core::FlowFile> &flow_file) {
  storage::DeleteAzureBlobStorageParameters params;
  if (!setBlobOperationParameters(params, context, flow_file)) {
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

REGISTER_RESOURCE(DeleteAzureBlobStorage, Processor);

}  // namespace org::apache::nifi::minifi::azure::processors
