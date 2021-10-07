/**
 * @file AzureDataLakeStorageProcessor.cpp
 * AzureDataLakeStorageProcessor class implementation
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

#include "AzureDataLakeStorageProcessor.h"

#include "utils/ProcessorConfigUtils.h"
#include "controllerservices/AzureStorageCredentialsService.h"

namespace org::apache::nifi::minifi::azure::processors {

const core::Property AzureDataLakeStorageProcessor::AzureStorageCredentialsService(
    core::PropertyBuilder::createProperty("Azure Storage Credentials Service")
      ->withDescription("Name of the Azure Storage Credentials Service used to retrieve the connection string from.")
      ->isRequired(true)
      ->build());
const core::Property AzureDataLakeStorageProcessor::FilesystemName(
    core::PropertyBuilder::createProperty("Filesystem Name")
      ->withDescription("Name of the Azure Storage File System. It is assumed to be already existing.")
      ->supportsExpressionLanguage(true)
      ->isRequired(true)
      ->build());
const core::Property AzureDataLakeStorageProcessor::DirectoryName(
    core::PropertyBuilder::createProperty("Directory Name")
      ->withDescription("Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. "
                        "If left empty it designates the root directory. The directory will be created if not already existing.")
      ->supportsExpressionLanguage(true)
      ->build());
const core::Property AzureDataLakeStorageProcessor::FileName(
    core::PropertyBuilder::createProperty("File Name")
      ->withDescription("The filename in Azure Storage. If left empty the filename attribute will be used by default.")
      ->supportsExpressionLanguage(true)
      ->build());

void AzureDataLakeStorageProcessor::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  std::optional<storage::AzureStorageCredentials> credentials;
  std::tie(std::ignore, credentials) = getCredentialsFromControllerService(context);
  if (!credentials) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Azure Storage Credentials Service property missing or invalid");
  }

  if (!credentials->isValid()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Azure Storage Credentials Service properties are not set or invalid");
  }

  credentials_ = *credentials;
}

bool AzureDataLakeStorageProcessor::setCommonParameters(
    storage::AzureDataLakeStorageParameters& params, const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  params.credentials = credentials_;

  if (!context->getProperty(FilesystemName, params.file_system_name, flow_file) || params.file_system_name.empty()) {
    logger_->log_error("Filesystem Name '%s' is invalid or empty!", params.file_system_name);
    return false;
  }

  context->getProperty(DirectoryName, params.directory_name, flow_file);

  context->getProperty(FileName, params.filename, flow_file);
  if (params.filename.empty() && (!flow_file->getAttribute("filename", params.filename) || params.filename.empty())) {
    logger_->log_error("No File Name is set and default object key 'filename' attribute could not be found!");
    return false;
  }

  return true;
}

}  // namespace org::apache::nifi::minifi::azure::processors
