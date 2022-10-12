/**
 * @file S3Processor.h
 * Base S3 processor class declaration
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

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "aws/core/auth/AWSCredentialsProvider.h"

#include "S3Wrapper.h"
#include "AWSCredentialsProvider.h"
#include "core/Property.h"
#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/OptionalUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

namespace region {

constexpr const char *AF_SOUTH_1 = "af-south-1";
constexpr const char *AP_EAST_1 = "ap-east-1";
constexpr const char *AP_NORTHEAST_1 = "ap-northeast-1";
constexpr const char *AP_NORTHEAST_2 = "ap-northeast-2";
constexpr const char *AP_NORTHEAST_3 = "ap-northeast-3";
constexpr const char *AP_SOUTH_1 = "ap-south-1";
constexpr const char *AP_SOUTHEAST_1 = "ap-southeast-1";
constexpr const char *AP_SOUTHEAST_2 = "ap-southeast-2";
constexpr const char *CA_CENTRAL_1 = "ca-central-1";
constexpr const char *CN_NORTH_1 = "cn-north-1";
constexpr const char *CN_NORTHWEST_1 = "cn-northwest-1";
constexpr const char *EU_CENTRAL_1 = "eu-central-1";
constexpr const char *EU_NORTH_1 = "eu-north-1";
constexpr const char *EU_SOUTH_1 = "eu-south-1";
constexpr const char *EU_WEST_1 = "eu-west-1";
constexpr const char *EU_WEST_2 = "eu-west-2";
constexpr const char *EU_WEST_3 = "eu-west-3";
constexpr const char *ME_SOUTH_1 = "me-south-1";
constexpr const char *SA_EAST_1 = "sa-east-1";
constexpr const char *US_EAST_1 = "us-east-1";
constexpr const char *US_EAST_2 = "us-east-2";
constexpr const char *US_GOV_EAST_1 = "us-gov-east-1";
constexpr const char *US_GOV_WEST_1 = "us-gov-west-1";
constexpr const char *US_WEST_1 = "us-west-1";
constexpr const char *US_WEST_2 = "us-west-2";

}  // namespace region

struct CommonProperties {
  std::string bucket;
  std::string object_key;
  Aws::Auth::AWSCredentials credentials;
  aws::s3::ProxyOptions proxy;
  std::string endpoint_override_url;
};

class S3Processor : public core::Processor {
 public:
  static const std::set<std::string> REGIONS;

  EXTENSIONAPI static const core::Property Bucket;
  EXTENSIONAPI static const core::Property AccessKey;
  EXTENSIONAPI static const core::Property SecretKey;
  EXTENSIONAPI static const core::Property CredentialsFile;
  EXTENSIONAPI static const core::Property AWSCredentialsProviderService;
  EXTENSIONAPI static const core::Property Region;
  EXTENSIONAPI static const core::Property CommunicationsTimeout;
  EXTENSIONAPI static const core::Property EndpointOverrideURL;
  EXTENSIONAPI static const core::Property ProxyHost;
  EXTENSIONAPI static const core::Property ProxyPort;
  EXTENSIONAPI static const core::Property ProxyUsername;
  EXTENSIONAPI static const core::Property ProxyPassword;
  EXTENSIONAPI static const core::Property UseDefaultCredentials;
  static auto properties() {
    return std::array{
      Bucket,
      AccessKey,
      SecretKey,
      CredentialsFile,
      AWSCredentialsProviderService,
      Region,
      CommunicationsTimeout,
      EndpointOverrideURL,
      ProxyHost,
      ProxyPort,
      ProxyUsername,
      ProxyPassword,
      UseDefaultCredentials
    };
  }

  explicit S3Processor(std::string name, const minifi::utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger);

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 protected:
  explicit S3Processor(const std::string& name, const minifi::utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender);

  std::optional<Aws::Auth::AWSCredentials> getAWSCredentialsFromControllerService(const std::shared_ptr<core::ProcessContext> &context) const;
  std::optional<Aws::Auth::AWSCredentials> getAWSCredentials(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file);
  std::optional<aws::s3::ProxyOptions> getProxy(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file);
  std::optional<CommonProperties> getCommonELSupportedProperties(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file);

  std::shared_ptr<core::logging::Logger> logger_;
  aws::s3::S3Wrapper s3_wrapper_;
  std::optional<Aws::Client::ClientConfiguration> client_config_;
};

}  // namespace org::apache::nifi::minifi::aws::processors
