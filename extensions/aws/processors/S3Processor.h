/**
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

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "AWSCredentialsProvider.h"
#include "AwsProcessor.h"
#include "S3Wrapper.h"
#include "aws/core/auth/AWSCredentialsProvider.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "utils/OptionalUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

class S3Processor : public AwsProcessor {
 public:
  EXTENSIONAPI static constexpr auto Bucket = core::PropertyDefinitionBuilder<>::createProperty("Bucket")
      .withDescription("The S3 bucket")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .supportsExpressionLanguage(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = minifi::utils::array_cat(AwsProcessor::Properties, std::to_array<core::PropertyReference>({Bucket}));

  using AwsProcessor::AwsProcessor;
  ~S3Processor() override = default;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  explicit S3Processor(core::ProcessorMetadata metadata, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender);

  aws::s3::S3Wrapper s3_wrapper_;
};

}  // namespace org::apache::nifi::minifi::aws::processors
