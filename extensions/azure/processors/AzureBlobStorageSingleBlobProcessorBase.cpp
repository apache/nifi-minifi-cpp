/**
 * @file AzureBlobStorageSingleBlobProcessorBase.cpp
 * AzureBlobStorageSingleBlobProcessorBase class implementation
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
