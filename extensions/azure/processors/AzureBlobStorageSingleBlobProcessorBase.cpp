// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "AzureBlobStorageSingleBlobProcessorBase.h"

#include "minifi-cpp/core/ProcessContext.h"

namespace org::apache::nifi::minifi::azure::processors {

bool AzureBlobStorageSingleBlobProcessorBase::setBlobOperationParameters(
    storage::AzureBlobStorageBlobOperationParameters& params,
    core::ProcessContext &context,
    const core::FlowFile& flow_file) {
  if (!setCommonStorageParameters(params, context, &flow_file)) {
    return false;
  }

  params.blob_name = context.getProperty(Blob, &flow_file).value_or("");
  if (params.blob_name.empty() && (!flow_file.getAttribute("filename", params.blob_name) || params.blob_name.empty())) {
    logger_->log_error("Blob is not set and default 'filename' attribute could not be found!");
    return false;
  }

  return true;
}

}  // namespace org::apache::nifi::minifi::azure::processors
