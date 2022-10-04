/**
 * @file PutAzureDataLakeStorage.cpp
 * PutAzureDataLakeStorage class implementation
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

#include "PutAzureDataLakeStorage.h"

#include <vector>

#include "utils/ProcessorConfigUtils.h"
#include "utils/gsl.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::azure::processors {

void PutAzureDataLakeStorage::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void PutAzureDataLakeStorage::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) {
  gsl_Expects(context && sessionFactory);
  AzureDataLakeStorageFileProcessorBase::onSchedule(context, sessionFactory);
  std::optional<storage::AzureStorageCredentials> credentials;
  std::tie(std::ignore, credentials) = getCredentialsFromControllerService(*context);
  if (!credentials) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Azure Storage Credentials Service property missing or invalid");
  }

  if (!credentials->isValid()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Azure Storage Credentials Service properties are not set or invalid");
  }

  credentials_ = *credentials;
  conflict_resolution_strategy_ = utils::parseEnumProperty<FileExistsResolutionStrategy>(*context, ConflictResolutionStrategy);
}

std::optional<storage::PutAzureDataLakeStorageParameters> PutAzureDataLakeStorage::buildUploadParameters(
    core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  storage::PutAzureDataLakeStorageParameters params;
  if (!setFileOperationCommonParameters(params, context, flow_file)) {
    return std::nullopt;
  }
  params.replace_file = conflict_resolution_strategy_ == FileExistsResolutionStrategy::REPLACE_FILE;

  return params;
}

void PutAzureDataLakeStorage::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session);
  logger_->log_trace("PutAzureDataLakeStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  const auto params = buildUploadParameters(*context, flow_file);
  if (!params) {
    session->transfer(flow_file, Failure);
    return;
  }

  PutAzureDataLakeStorage::ReadCallback callback(flow_file->getSize(), azure_data_lake_storage_, *params, logger_);
  session->read(flow_file, std::ref(callback));
  const storage::UploadDataLakeStorageResult result = callback.getResult();

  if (result.result_code == storage::UploadResultCode::FILE_ALREADY_EXISTS) {
    gsl_Expects(conflict_resolution_strategy_ != FileExistsResolutionStrategy::REPLACE_FILE);
    if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::FAIL_FLOW) {
      logger_->log_error("Failed to upload file '%s/%s' to filesystem '%s' on Azure Data Lake storage because file already exists",
        params->directory_name, params->filename, params->file_system_name);
      session->transfer(flow_file, Failure);
      return;
    } else if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::IGNORE_REQUEST) {
      logger_->log_debug("Upload of file '%s/%s' was ignored because it already exits in filesystem '%s' on Azure Data Lake Storage",
        params->directory_name, params->filename, params->file_system_name);
      session->transfer(flow_file, Success);
      return;
    }
  } else if (result.result_code == storage::UploadResultCode::FAILURE) {
    logger_->log_error("Failed to upload file '%s/%s' to filesystem '%s' on Azure Data Lake storage", params->directory_name, params->filename, params->file_system_name);
    session->transfer(flow_file, Failure);
  } else {
    session->putAttribute(flow_file, "azure.filesystem", params->file_system_name);
    session->putAttribute(flow_file, "azure.directory", params->directory_name);
    session->putAttribute(flow_file, "azure.filename", params->filename);
    session->putAttribute(flow_file, "azure.primaryUri", result.primary_uri);
    session->putAttribute(flow_file, "azure.length", std::to_string(flow_file->getSize()));
    logger_->log_debug("Successfully uploaded file '%s/%s' to filesystem '%s' on Azure Data Lake storage", params->directory_name, params->filename, params->file_system_name);
    session->transfer(flow_file, Success);
  }
}

PutAzureDataLakeStorage::ReadCallback::ReadCallback(
      uint64_t flow_size, storage::AzureDataLakeStorage& azure_data_lake_storage, const storage::PutAzureDataLakeStorageParameters& params, std::shared_ptr<core::logging::Logger> logger)
  : flow_size_(flow_size),
    azure_data_lake_storage_(azure_data_lake_storage),
    params_(params),
    logger_(std::move(logger)) {
}

int64_t PutAzureDataLakeStorage::ReadCallback::operator()(const std::shared_ptr<io::InputStream>& stream) {
  std::vector<std::byte> buffer;
  buffer.resize(flow_size_);
  size_t read_ret = stream->read(buffer);
  if (io::isError(read_ret) || read_ret != flow_size_) {
    return -1;
  }

  result_ = azure_data_lake_storage_.uploadFile(params_, buffer);
  return read_ret;
}

}  // namespace org::apache::nifi::minifi::azure::processors
