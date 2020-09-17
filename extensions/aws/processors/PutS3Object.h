/**
 * @file PutS3Object.h
 * PutS3Object class declaration
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

#include "S3Wrapper.h"
#include "core/Property.h"
#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "utils/OptionalUtils.h"

#include <aws/core/auth/AWSCredentialsProvider.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

namespace storage_class
{

  constexpr const char *STANDARD = "Standard";
  constexpr const char *REDUCED_REDUNDANCY = "ReducedRedundancy";

} // namespace storage_class

namespace region
{

  constexpr const char *US_GOV_WEST_1 = "us-gov-west-1";
  constexpr const char *US_EAST_1 = "us-east-1";
  constexpr const char *US_EAST_2 = "us-east-2";
  constexpr const char *US_WEST_1 = "us-west-1";
  constexpr const char *US_WEST_2 = "us-west-2";
  constexpr const char *EU_WEST_1 = "eu-west-1";
  constexpr const char *EU_WEST_2 = "eu-west-2";
  constexpr const char *EU_CENTRAL_1 = "eu-central-1";
  constexpr const char *AP_SOUTH_1 = "ap-south-1";
  constexpr const char *AP_SOUTHEAST_1 = "ap-southeast-1";
  constexpr const char *AP_SOUTHEAST_2 = "ap-southeast-2";
  constexpr const char *AP_NORTHEAST_1 = "ap-northeast-1";
  constexpr const char *AP_NORTHEAST_2 = "ap-northeast-2";
  constexpr const char *SA_EAST_1 = "sa-east-1";
  constexpr const char *CN_NORTH_1 = "cn-north-1";
  constexpr const char *CA_CENTRAL_1 = "ca-central-1";

} // namespace region


class PutS3Object : public core::Processor {
public:
  static constexpr char const* ProcessorName = "PutS3Object";

  // Supported Properties
  static const core::Property ObjectKey;
  static const core::Property Bucket;
  static const core::Property ContentType;
  static const core::Property AccessKey;
  static const core::Property SecretKey;
  static const core::Property CredentialsFile;
  static const core::Property AWSCredentialsProviderService;
  static const core::Property StorageClass;
  static const core::Property Region;
  static const core::Property CommunicationsTimeout;
  static const core::Property EndpointOverrideURL;
  // static const core::Property ProxyHost;
  // static const core::Property ProxyHostPort;

  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  explicit PutS3Object(std::string name, utils::Identifier uuid = utils::Identifier())
      : PutS3Object(name, uuid, utils::make_unique<S3Wrapper>()) {
  }

  explicit PutS3Object(std::string name, utils::Identifier uuid, std::unique_ptr<aws::processors::AbstractS3Wrapper> s3_wrapper)
      : core::Processor(std::move(name), uuid)
      , s3_wrapper_(std::move(s3_wrapper)) {
  }

  ~PutS3Object() override = default;

  bool supportsDynamicProperties() override { return true; }
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void notifyStop() override;

private:
  utils::optional<Aws::Auth::AWSCredentials> getAWSCredentialsFromControllerService(const std::shared_ptr<core::ProcessContext> &context);
  utils::optional<Aws::Auth::AWSCredentials> getAWSCredentialsFromProperties(const std::shared_ptr<core::ProcessContext> &context);
  utils::optional<Aws::Auth::AWSCredentials> getAWSCredentialsFromFile(const std::shared_ptr<core::ProcessContext> &context);
  Aws::Auth::AWSCredentials getAWSCredentials(const std::shared_ptr<core::ProcessContext> &context);

  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<PutS3Object>::getLogger()};
  std::string object_key_;
  std::string bucket_;
  std::string content_type_ = "application/octet-stream";
  std::unique_ptr<aws::processors::AbstractS3Wrapper> s3_wrapper_;
};

REGISTER_RESOURCE(PutS3Object, "This Processor puts FlowFiles to an Amazon S3 Bucket.");

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
