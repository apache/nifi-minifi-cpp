// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "AzureDataLakeStorageFileProcessorBase.h"

#include "utils/ProcessorConfigUtils.h"
#include "controllerservices/AzureStorageCredentialsService.h"

namespace org::apache::nifi::minifi::azure::processors {

bool AzureDataLakeStorageFileProcessorBase::setFileOperationCommonParameters(
    storage::AzureDataLakeStorageFileOperationParameters& params, core::ProcessContext& context, const core::FlowFile& flow_file) {
  if (!setCommonParameters(params, context, &flow_file)) {
    return false;
  }

  params.filename = context.getProperty(FileName, &flow_file).value_or("");
  if (params.filename.empty() && (!flow_file.getAttribute("filename", params.filename) || params.filename.empty())) {
    logger_->log_error("No File Name is set and default object key 'filename' attribute could not be found!");
    return false;
  }

  return true;
}

}  // namespace org::apache::nifi::minifi::azure::processors
