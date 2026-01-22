/**
 * @file AzureDataLakeStorageProcessorBase.cpp
 * AzureDataLakeStorageProcessorBase class implementation
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

#include "AzureDataLakeStorageProcessorBase.h"

#include "utils/ProcessorConfigUtils.h"
#include "controllerservices/AzureStorageCredentialsService.h"

namespace org::apache::nifi::minifi::azure::processors {

void AzureDataLakeStorageProcessorBase::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  AzureStorageProcessorBase::onSchedule(context, session_factory);
  std::optional<storage::AzureStorageCredentials> credentials;
  std::tie(std::ignore, credentials) = getCredentialsFromControllerService(context);
  if (!credentials) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Azure Storage Credentials Service property missing or invalid");
  }

  if (!credentials->isValid()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Credentials set in the Azure Storage credentials service are invalid");
  }

  credentials_ = *credentials;
}

bool AzureDataLakeStorageProcessorBase::setCommonParameters(storage::AzureDataLakeStorageParameters& params,
      core::ProcessContext& context,
      const core::FlowFile* const flow_file) {
  params.credentials = credentials_;

  params.file_system_name = context.getProperty(FilesystemName, flow_file).value_or("");
  if (params.file_system_name.empty()) {
    logger_->log_error("Filesystem Name '{}' is invalid or empty!", params.file_system_name);
    return false;
  }

  params.directory_name = context.getProperty(DirectoryName, flow_file).value_or("");
  params.proxy_configuration = proxy_configuration_;

  return true;
}

}  // namespace org::apache::nifi::minifi::azure::processors
