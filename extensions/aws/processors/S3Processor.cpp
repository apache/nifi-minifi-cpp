/**
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

#include "S3Processor.h"

#include <memory>
#include <string>
#include <utility>

#include "AWSCredentialsService.h"
#include "S3Wrapper.h"
#include "core/ProcessContext.h"
#include "properties/Properties.h"
#include "range/v3/algorithm/contains.hpp"
#include "utils/HTTPUtils.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

S3Processor::S3Processor(const std::string_view name, const minifi::utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger)
  : AwsProcessor(name, uuid, std::move(logger)) {}

S3Processor::S3Processor(const std::string_view name, const minifi::utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
  : AwsProcessor(name, uuid, std::move(logger)),
    s3_wrapper_(std::move(s3_request_sender)) {
}

void S3Processor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  AwsProcessor::onSchedule(context, session_factory);
  if (!getProperty(Bucket.name)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bucket property missing or invalid");
  }
}

}  // namespace org::apache::nifi::minifi::aws::processors
