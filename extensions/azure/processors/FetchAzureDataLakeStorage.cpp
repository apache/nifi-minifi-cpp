/**
 * @file FetchAzureDataLakeStorage.cpp
 * FetchAzureDataLakeStorage class implementation
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

#include "FetchAzureDataLakeStorage.h"

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::azure::processors {

void FetchAzureDataLakeStorage::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

std::optional<storage::FetchAzureDataLakeStorageParameters> FetchAzureDataLakeStorage::buildFetchParameters(
    core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  storage::FetchAzureDataLakeStorageParameters params;
  if (!setFileOperationCommonParameters(params, context, flow_file)) {
    return std::nullopt;
  }

  std::string value;
  if (context.getProperty(RangeStart, value, flow_file)) {
    params.range_start = std::stoull(value);
    logger_->log_debug("Range Start property set to %llu", *params.range_start);
  }

  if (context.getProperty(RangeLength, value, flow_file)) {
    params.range_length = std::stoull(value);
    logger_->log_debug("Range Length property set to %llu", *params.range_length);
  }

  if (context.getProperty(NumberOfRetries, value, flow_file)) {
    params.number_of_retries = std::stoull(value);
    logger_->log_debug("Number Of Retries property set to %llu", *params.number_of_retries);
  }

  return params;
}

void FetchAzureDataLakeStorage::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session);
  logger_->log_trace("FetchAzureDataLakeStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  const auto params = buildFetchParameters(*context, flow_file);
  if (!params) {
    session->transfer(flow_file, Failure);
    return;
  }

  auto fetched_flow_file = session->create(flow_file);
  std::optional<uint64_t> result;
  session->write(fetched_flow_file, [&, this](const std::shared_ptr<io::OutputStream>& output_stream) -> int64_t {
    result = azure_data_lake_storage_.fetchFile(*params, *output_stream);
    if (!result) {
      return 0;
    }

    return gsl::narrow<int64_t>(*result);
  });

  if (result == std::nullopt) {
    logger_->log_error("Failed to fetch file '%s' from Azure Data Lake storage", params->filename);
    session->transfer(flow_file, Failure);
    session->remove(fetched_flow_file);
  } else {
    logger_->log_debug("Successfully fetched file '%s' from filesystem '%s' on Azure Data Lake storage", params->filename, params->file_system_name);
    session->transfer(fetched_flow_file, Success);
    session->remove(flow_file);
  }
}

REGISTER_RESOURCE(FetchAzureDataLakeStorage, Processor);

}  // namespace org::apache::nifi::minifi::azure::processors
