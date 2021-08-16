/**
 * @file DeleteAzureDataLakeStorage.cpp
 * DeleteAzureDataLakeStorage class implementation
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

#include "DeleteAzureDataLakeStorage.h"

#include "utils/ProcessorConfigUtils.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace processors {

const core::Property DeleteAzureDataLakeStorage::FileName(
    core::PropertyBuilder::createProperty("File Name")
      ->withDescription("The filename")
      ->supportsExpressionLanguage(true)
      ->build());

const core::Relationship DeleteAzureDataLakeStorage::Success("success", "Files that have been successfully written to Azure storage are transferred to this relationship");
const core::Relationship DeleteAzureDataLakeStorage::Failure("failure", "Files that could not be written to Azure storage for some reason are transferred to this relationship");

void DeleteAzureDataLakeStorage::initialize() {
  AzureDataLakeStorageProcessor::initialize();
  // Add new supported properties
  updateSupportedProperties({
    FileName
  });
  // Set the supported relationships
  setSupportedRelationships({
    Success,
    Failure
  });
}

std::optional<storage::DeleteAzureDataLakeStorageParameters> DeleteAzureDataLakeStorage::buildDeleteParameters(
    const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  storage::DeleteAzureDataLakeStorageParameters params;
  params.connection_string = connection_string_;

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

void DeleteAzureDataLakeStorage::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  logger_->log_debug("DeleteAzureDataLakeStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  const auto params = buildDeleteParameters(context, flow_file);
  if (!params) {
    session->transfer(flow_file, Failure);
    return;
  }

  if (!azure_data_lake_storage_.deleteFile(*params)) {
    logger_->log_error("Failed to delete file '%s' to Azure Data Lake storage", params->filename);
    session->transfer(flow_file, Failure);
  } else {
    logger_->log_debug("Successfully deleted file '%s' of filesystem '%s' on Azure Data Lake storage", params->filename, params->file_system_name);
    session->transfer(flow_file, Success);
  }
}

}  // namespace processors
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
