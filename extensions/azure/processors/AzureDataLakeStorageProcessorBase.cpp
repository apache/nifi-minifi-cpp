// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
