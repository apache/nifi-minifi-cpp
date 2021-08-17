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

#include "utils/ProcessorConfigUtils.h"
#include "utils/gsl.h"
#include "controllerservices/AzureStorageCredentialsService.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace processors {

const core::Property PutAzureDataLakeStorage::AzureStorageCredentialsService(
    core::PropertyBuilder::createProperty("Azure Storage Credentials Service")
      ->withDescription("Name of the Azure Storage Credentials Service used to retrieve the connection string from.")
      ->isRequired(true)
      ->build());
const core::Property PutAzureDataLakeStorage::FilesystemName(
    core::PropertyBuilder::createProperty("Filesystem Name")
      ->withDescription("Name of the Azure Storage File System. It is assumed to be already existing.")
      ->supportsExpressionLanguage(true)
      ->isRequired(true)
      ->build());
const core::Property PutAzureDataLakeStorage::DirectoryName(
    core::PropertyBuilder::createProperty("Directory Name")
      ->withDescription("Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. "
                        "If left empty it designates the root directory. The directory will be created if not already existing.")
      ->supportsExpressionLanguage(true)
      ->build());
const core::Property PutAzureDataLakeStorage::FileName(
    core::PropertyBuilder::createProperty("File Name")
      ->withDescription("The filename")
      ->supportsExpressionLanguage(true)
      ->build());
const core::Property PutAzureDataLakeStorage::ConflictResolutionStrategy(
    core::PropertyBuilder::createProperty("Conflict Resolution Strategy")
      ->withDescription("Indicates what should happen when a file with the same name already exists in the output directory.")
      ->isRequired(true)
      ->withDefaultValue<std::string>(toString(FileExistsResolutionStrategy::FAIL_FLOW))
      ->withAllowableValues<std::string>(FileExistsResolutionStrategy::values())
      ->build());

const core::Relationship PutAzureDataLakeStorage::Success("success", "Files that have been successfully written to Azure storage are transferred to this relationship");
const core::Relationship PutAzureDataLakeStorage::Failure("failure", "Files that could not be written to Azure storage for some reason are transferred to this relationship");

void PutAzureDataLakeStorage::initialize() {
  // Set the supported properties
  setSupportedProperties({
    AzureStorageCredentialsService,
    FilesystemName,
    DirectoryName,
    FileName,
    ConflictResolutionStrategy
  });
  // Set the supported relationships
  setSupportedRelationships({
    Success,
    Failure
  });
}

std::string PutAzureDataLakeStorage::getConnectionStringFromControllerService(const std::shared_ptr<core::ProcessContext> &context) const {
  std::string service_name;
  if (!context->getProperty(AzureStorageCredentialsService.getName(), service_name) || service_name.empty()) {
    return "";
  }

  auto service = context->getControllerService(service_name);
  if (nullptr == service) {
    logger_->log_error("Azure Storage credentials service with name: '%s' could not be found", service_name.c_str());
    return "";
  }

  auto azure_credentials_service = std::dynamic_pointer_cast<minifi::azure::controllers::AzureStorageCredentialsService>(service);
  if (!azure_credentials_service) {
    logger_->log_error("Controller service with name: '%s' is not an Azure Storage credentials service", service_name.c_str());
    return "";
  }

  return azure_credentials_service->getConnectionString();
}

void PutAzureDataLakeStorage::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  connection_string_ = getConnectionStringFromControllerService(context);
  if (connection_string_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Azure Storage Credentials Service property missing or invalid");
  }

  conflict_resolution_strategy_ = FileExistsResolutionStrategy::parse(
    utils::parsePropertyWithAllowableValuesOrThrow(*context, ConflictResolutionStrategy.getName(), FileExistsResolutionStrategy::values()).c_str());
}

std::optional<storage::PutAzureDataLakeStorageParameters> PutAzureDataLakeStorage::buildUploadParameters(
    const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  storage::PutAzureDataLakeStorageParameters params;
  params.connection_string = connection_string_;
  params.replace_file = conflict_resolution_strategy_ == FileExistsResolutionStrategy::REPLACE_FILE;

  if (!context->getProperty(FilesystemName, params.file_system_name, flow_file) || params.file_system_name.empty()) {
    logger_->log_error("Filesystem Name '%s' is invalid or empty!", params.file_system_name);
    return std::nullopt;
  }

  context->getProperty(DirectoryName, params.directory_name, flow_file);

  context->getProperty(FileName, params.filename, flow_file);
  if (params.filename.empty() && (!flow_file->getAttribute("filename", params.filename) || params.filename.empty())) {
    logger_->log_error("No File Name is set and default object key 'filename' attribute could not be found!");
    return std::nullopt;
  }

  return params;
}

void PutAzureDataLakeStorage::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  logger_->log_debug("PutAzureDataLakeStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  const auto params = buildUploadParameters(context, flow_file);
  if (!params) {
    session->transfer(flow_file, Failure);
    return;
  }

  PutAzureDataLakeStorage::ReadCallback callback(flow_file->getSize(), azure_data_lake_storage_, *params, logger_);
  session->read(flow_file, &callback);
  if (callback.caughtFileAlreadyExistsError()) {
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
  }

  auto result = callback.getResult();
  if (!result) {
    logger_->log_error("Failed to upload file '%s/%s' to filesystem '%s' on Azure Data Lake storage", params->directory_name, params->filename, params->file_system_name);
    session->transfer(flow_file, Failure);
  } else {
    session->putAttribute(flow_file, "azure.filesystem", params->file_system_name);
    session->putAttribute(flow_file, "azure.directory", params->directory_name);
    session->putAttribute(flow_file, "azure.filename", params->filename);
    session->putAttribute(flow_file, "azure.primaryUri", result->primary_uri);
    session->putAttribute(flow_file, "azure.length", std::to_string(result->length));
    logger_->log_debug("Successfully uploaded file '%s/%s' to filesystem '%s' on Azure Data Lake storage", params->directory_name, params->filename, params->file_system_name);
    session->transfer(flow_file, Success);
  }
}

PutAzureDataLakeStorage::ReadCallback::ReadCallback(
    uint64_t flow_size, storage::AzureDataLakeStorage& azure_data_lake_storage, const storage::PutAzureDataLakeStorageParameters& params, std::shared_ptr<logging::Logger> logger)
  : flow_size_(flow_size)
  , azure_data_lake_storage_(azure_data_lake_storage)
  , params_(params)
  , logger_(std::move(logger)) {
}

int64_t PutAzureDataLakeStorage::ReadCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  std::vector<uint8_t> buffer;
  int read_ret = stream->read(buffer, flow_size_);
  if (io::isError(read_ret)) {
    return -1;
  }

  try {
    result_ = azure_data_lake_storage_.uploadFile(params_, buffer.data(), flow_size_);
  } catch(const storage::AzureDataLakeStorage::FileAlreadyExistsException& ex) {
    logger_->log_warn(ex.what());
    caught_file_already_exists_error_ = true;
  } catch(const std::runtime_error& err) {
    logger_->log_error("A runtime error occurred while uploading file to Azure Data Lake storage: %s", err.what());
    return read_ret;
  }

  return read_ret;
}

}  // namespace processors
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
