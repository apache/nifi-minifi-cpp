// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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

constexpr std::string_view S3_EXCEPTION = "s3.exception";
constexpr std::string_view S3_ERROR_MESSAGE = "s3.errorMessage";
constexpr std::string_view S3_ERROR_RETRYABLE = "s3.errorRetryable";
constexpr std::string_view S3_STATUS_CODE = "s3.statusCode";

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
  using S3WrapperFactory =
    std::function<std::unique_ptr<aws::s3::S3Wrapper>(const Aws::Auth::AWSCredentials& credentials, const Aws::Client::ClientConfiguration& client_config, bool use_virtual_addressing)>;
  explicit S3Processor(core::ProcessorMetadata metadata, S3WrapperFactory s3_wrapper_factory);

  S3WrapperFactory s3_wrapper_factory_ =
    [](const Aws::Auth::AWSCredentials& credentials, const Aws::Client::ClientConfiguration& client_config, bool use_virtual_addressing) {
      return std::make_unique<aws::s3::S3Wrapper>(credentials, client_config, use_virtual_addressing);
    };

  static void setFailureFlowFileAttributes(core::ProcessSession& session, core::FlowFile& flow_file, const s3::S3Error& s3_error);
  std::unique_ptr<aws::s3::S3Wrapper> s3_wrapper_;
};

}  // namespace org::apache::nifi::minifi::aws::processors
