/**
 * @file FetchAzureBlobStorage.cpp
 * FetchAzureBlobStorage class implementation
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

#include "FetchAzureBlobStorage.h"

#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "io/StreamPipe.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

void FetchAzureBlobStorage::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

std::optional<storage::FetchAzureBlobStorageParameters> FetchAzureBlobStorage::buildFetchAzureBlobStorageParameters(
    core::ProcessContext &context, const core::FlowFile& flow_file) {
  storage::FetchAzureBlobStorageParameters params;
  if (!setBlobOperationParameters(params, context, flow_file)) {
    return std::nullopt;
  }

  std::string value;
  if (auto range_start = utils::parseOptionalU64Property(context, RangeStart, &flow_file)) {
    params.range_start = *range_start;
    logger_->log_debug("Range Start property set to {}", *params.range_start);
  }

  if (auto range_length = utils::parseOptionalU64Property(context, RangeLength, &flow_file)) {
    params.range_length = *range_length;
    logger_->log_debug("Range Length property set to {}", *params.range_length);
  }

  return params;
}

void FetchAzureBlobStorage::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchAzureBlobStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  const auto params = buildFetchAzureBlobStorageParameters(context, *flow_file);
  if (!params) {
    session.transfer(flow_file, Failure);
    return;
  }

  auto fetched_flow_file = session.create(flow_file.get());
  std::optional<int64_t> result_size;
  session.write(fetched_flow_file, [&, this](const std::shared_ptr<io::OutputStream>& stream) -> int64_t {
    result_size = azure_blob_storage_.fetchBlob(*params, *stream);
    if (!result_size) {
      return 0;
    }
    return gsl::narrow<int64_t>(*result_size);
  });

  if (result_size == std::nullopt) {
    logger_->log_error("Failed to fetch blob '{}' from Azure Blob storage", params->blob_name);
    session.transfer(flow_file, Failure);
    session.remove(fetched_flow_file);
  } else {
    logger_->log_debug("Successfully fetched blob '{}' from container '{}' on Azure Blob storage", params->blob_name, params->container_name);
    session.transfer(fetched_flow_file, Success);
    session.remove(flow_file);
  }
}

REGISTER_RESOURCE(FetchAzureBlobStorage, Processor);

}  // namespace org::apache::nifi::minifi::azure::processors
