// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "FetchAzureDataLakeStorage.h"

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

void FetchAzureDataLakeStorage::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

std::optional<storage::FetchAzureDataLakeStorageParameters> FetchAzureDataLakeStorage::buildFetchParameters(
    core::ProcessContext& context, const core::FlowFile& flow_file) {
  storage::FetchAzureDataLakeStorageParameters params;
  if (!setFileOperationCommonParameters(params, context, flow_file)) {
    return std::nullopt;
  }

  if (auto range_start = utils::parseOptionalU64Property(context, RangeStart, &flow_file)) {
    params.range_start = range_start;
    logger_->log_debug("Range Start property set to {}", *params.range_start);
  }

  if (auto range_length = utils::parseOptionalU64Property(context, RangeLength, &flow_file)) {
    params.range_length = range_length;
    logger_->log_debug("Range Length property set to {}", *params.range_length);
  }

  if (auto number_of_retries = utils::parseOptionalU64Property(context, NumberOfRetries, &flow_file)) {
    params.number_of_retries = number_of_retries;
    logger_->log_debug("Number Of Retries property set to {}", *params.number_of_retries);
  }

  return params;
}

void FetchAzureDataLakeStorage::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchAzureDataLakeStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  const auto params = buildFetchParameters(context, *flow_file);
  if (!params) {
    session.transfer(flow_file, Failure);
    return;
  }

  auto fetched_flow_file = session.create(flow_file.get());
  std::optional<uint64_t> result;
  session.write(fetched_flow_file, [&, this](const std::shared_ptr<io::OutputStream>& output_stream) -> io::IoResult {
    result = azure_data_lake_storage_.fetchFile(*params, *output_stream);
    return io::IoResult::from(result.value_or(0));
  });

  if (result == std::nullopt) {
    logger_->log_error("Failed to fetch file '{}' from Azure Data Lake storage", params->filename);
    session.transfer(flow_file, Failure);
    session.remove(fetched_flow_file);
  } else {
    logger_->log_debug("Successfully fetched file '{}' from filesystem '{}' on Azure Data Lake storage", params->filename, params->file_system_name);
    session.transfer(fetched_flow_file, Success);
    session.remove(flow_file);
  }
}

REGISTER_RESOURCE(FetchAzureDataLakeStorage, Processor);

}  // namespace org::apache::nifi::minifi::azure::processors
