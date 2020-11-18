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

#include <sstream>
#include <utility>
#include <vector>
#include <memory>
#include <string>
#include <set>
#include <algorithm>

#include "S3Wrapper.h"
#include "core/Property.h"
#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/OptionalUtils.h"
#include "AWSCredentialsProvider.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

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

class PutS3Object : public core::Processor {
 public:
  static constexpr char const* ProcessorName = "PutS3Object";

  static const std::set<std::string> CANNED_ACLS;
  static const std::set<std::string> REGIONS;
  static const std::set<std::string> STORAGE_CLASSES;
  static const std::set<std::string> SERVER_SIDE_ENCRYPTIONS;

  // Supported Properties
  static const core::Property ObjectKey;
  static const core::Property Bucket;
  static const core::Property ContentType;
  static const core::Property AccessKey;
  static const core::Property SecretKey;
  static const core::Property CredentialsFile;
  static const core::Property AWSCredentialsProviderService;
  static const core::Property StorageClass;
  static const core::Property ServerSideEncryption;
  static const core::Property Region;
  static const core::Property CommunicationsTimeout;
  static const core::Property FullControlUserList;
  static const core::Property ReadPermissionUserList;
  static const core::Property ReadACLUserList;
  static const core::Property WriteACLUserList;
  static const core::Property CannedACL;
  static const core::Property EndpointOverrideURL;
  static const core::Property ProxyHost;
  static const core::Property ProxyPort;
  static const core::Property ProxyUsername;
  static const core::Property ProxyPassword;
  static const core::Property UseDefaultCredentials;

  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  explicit PutS3Object(std::string name, minifi::utils::Identifier uuid = minifi::utils::Identifier())
      : PutS3Object(name, uuid, minifi::utils::make_unique<aws::s3::S3Wrapper>()) {
  }

  explicit PutS3Object(std::string name, minifi::utils::Identifier uuid, std::unique_ptr<aws::s3::S3WrapperBase> s3_wrapper)
      : core::Processor(std::move(name), uuid)
      , s3_wrapper_(std::move(s3_wrapper)) {
  }

  ~PutS3Object() override = default;

  bool supportsDynamicProperties() override { return true; }
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

  class ReadCallback : public InputStreamCallback {
   public:
    static const uint64_t MAX_SIZE;
    static const uint64_t BUFFER_SIZE;

    ReadCallback(uint64_t flow_size, const minifi::aws::s3::PutObjectRequestParameters& options, aws::s3::S3WrapperBase* s3_wrapper)
      : flow_size_(flow_size)
      , options_(options)
      , s3_wrapper_(s3_wrapper) {
    }

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      if (flow_size_ > MAX_SIZE) {
        return -1;
      }
      std::vector<uint8_t> buffer;
      auto data_stream = std::make_shared<std::stringstream>();
      buffer.reserve(BUFFER_SIZE);
      read_size_ = 0;
      while (read_size_ < flow_size_) {
        auto next_read_size = std::min(flow_size_ - read_size_, BUFFER_SIZE);
        int read_ret = stream->read(buffer.data(), next_read_size);
        if (read_ret < 0) {
          return -1;
        }
        if (read_ret > 0) {
          data_stream->write(reinterpret_cast<char*>(buffer.data()), next_read_size);
          read_size_ += read_ret;
        } else {
          break;
        }
      }
      result_ = s3_wrapper_->putObject(options_, data_stream);
      return read_size_;
    }

    uint64_t flow_size_;
    const minifi::aws::s3::PutObjectRequestParameters& options_;
    aws::s3::S3WrapperBase* s3_wrapper_;
    uint64_t read_size_ = 0;
    minifi::utils::optional<minifi::aws::s3::PutObjectResult> result_ = minifi::utils::nullopt;
  };

 private:
  minifi::utils::optional<Aws::Auth::AWSCredentials> getAWSCredentialsFromControllerService(const std::shared_ptr<core::ProcessContext> &context) const;
  minifi::utils::optional<Aws::Auth::AWSCredentials> getAWSCredentials(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file);
  void fillUserMetadata(const std::shared_ptr<core::ProcessContext> &context);
  bool setProxy(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file);
  bool getExpressionLanguageSupportedProperties(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file);
  std::string parseAccessControlList(const std::string &comma_separated_list) const;
  bool setCannedAcl(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file);
  bool setAccessControl(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file);
  void setAttributes(const std::shared_ptr<core::ProcessSession> &session,  const std::shared_ptr<core::FlowFile> &flow_file, const minifi::aws::s3::PutObjectResult &put_object_result);

  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<PutS3Object>::getLogger()};
  aws::s3::PutObjectRequestParameters put_s3_request_params_;
  std::unique_ptr<aws::s3::S3WrapperBase> s3_wrapper_;
  std::string user_metadata_;
  aws::AWSCredentialsProvider aws_credentials_provider_;
};

REGISTER_RESOURCE(PutS3Object, "This Processor puts FlowFiles to an Amazon S3 Bucket.");

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
