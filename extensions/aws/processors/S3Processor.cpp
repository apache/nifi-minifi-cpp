// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "S3Processor.h"

#include <memory>
#include <string>
#include <utility>

#include "AWSCredentialsService.h"
#include "S3Wrapper.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "range/v3/algorithm/contains.hpp"
#include "utils/HTTPUtils.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

S3Processor::S3Processor(core::ProcessorMetadata metadata, S3WrapperFactory s3_wrapper_factory)
    : AwsProcessor(std::move(metadata)),
      s3_wrapper_factory_(std::move(s3_wrapper_factory)) {
}

void S3Processor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  AwsProcessor::onSchedule(context, session_factory);
  if (!context.hasNonEmptyProperty(Bucket.name)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bucket property missing or invalid");
  }
}

void S3Processor::setFailureFlowFileAttributes(core::ProcessSession& session, core::FlowFile& flow_file, const s3::S3Error& s3_error) {
  session.putAttribute(flow_file, S3_EXCEPTION, s3_error.name);
  session.putAttribute(flow_file, S3_ERROR_MESSAGE, s3_error.message);
  session.putAttribute(flow_file, S3_ERROR_RETRYABLE, s3_error.is_retryable ? "true" : "false");
  session.putAttribute(flow_file, S3_STATUS_CODE, std::to_string(s3_error.http_code));
}

}  // namespace org::apache::nifi::minifi::aws::processors
