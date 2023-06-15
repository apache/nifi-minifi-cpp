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

#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "io/StreamPipe.h"
#include "S3Processor.h"
#include "utils/ArrayUtils.h"
#include "utils/gsl.h"
#include "utils/Id.h"

template<typename T>
class S3TestsFixture;

namespace org::apache::nifi::minifi::aws::processors {

class PutS3Object : public S3Processor {
 public:
  static constexpr auto CANNED_ACLS = minifi::utils::getKeys(minifi::aws::s3::CANNED_ACL_MAP);
  static constexpr auto STORAGE_CLASSES = minifi::utils::getKeys(minifi::aws::s3::STORAGE_CLASS_MAP);
  static constexpr auto SERVER_SIDE_ENCRYPTIONS = minifi::utils::getKeys(minifi::aws::s3::SERVER_SIDE_ENCRYPTION_MAP);

  EXTENSIONAPI static constexpr const char* Description = "Puts FlowFiles to an Amazon S3 Bucket. The upload uses the PutS3Object method. "
      "The PutS3Object method send the file in a single synchronous call, but it has a 5GB size limit. Larger files sent using the multipart upload methods are currently not supported. "
      "The AWS libraries select an endpoint URL based on the AWS region, but this can be overridden with the 'Endpoint Override URL' property for use with other S3-compatible endpoints. "
      "The S3 API specifies that the maximum file size for a PutS3Object upload is 5GB.";

  EXTENSIONAPI static constexpr auto ObjectKey = core::PropertyDefinitionBuilder<>::createProperty("Object Key")
      .withDescription("The key of the S3 object. If none is given the filename attribute will be used by default.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ContentType = core::PropertyDefinitionBuilder<>::createProperty("Content Type")
      .withDescription("Sets the Content-Type HTTP header indicating the type of content stored in "
          "the associated object. The value of this header is a standard MIME type. "
          "If no content type is provided the default content type "
          "\"application/octet-stream\" will be used.")
      .supportsExpressionLanguage(true)
      .withDefaultValue("application/octet-stream")
      .build();
  EXTENSIONAPI static constexpr auto StorageClass = core::PropertyDefinitionBuilder<STORAGE_CLASSES.size()>::createProperty("Storage Class")
      .withDescription("AWS S3 Storage Class")
      .isRequired(true)
      .withDefaultValue("Standard")
      .withAllowedValues(STORAGE_CLASSES)
      .build();
  EXTENSIONAPI static constexpr auto ServerSideEncryption = core::PropertyDefinitionBuilder<SERVER_SIDE_ENCRYPTIONS.size()>::createProperty("Server Side Encryption")
      .isRequired(true)
      .withDefaultValue("None")
      .withAllowedValues(SERVER_SIDE_ENCRYPTIONS)
      .withDescription("Specifies the algorithm used for server side encryption.")
      .build();
  EXTENSIONAPI static constexpr auto FullControlUserList = core::PropertyDefinitionBuilder<>::createProperty("FullControl User List")
      .withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Full Control for an object.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ReadPermissionUserList = core::PropertyDefinitionBuilder<>::createProperty("Read Permission User List")
      .withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Read Access for an object.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ReadACLUserList = core::PropertyDefinitionBuilder<>::createProperty("Read ACL User List")
      .withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to read "
          "the Access Control List for an object.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto WriteACLUserList = core::PropertyDefinitionBuilder<>::createProperty("Write ACL User List")
      .withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to change "
          "the Access Control List for an object.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto CannedACL = core::PropertyDefinitionBuilder<>::createProperty("Canned ACL")
      .withDescription("Amazon Canned ACL for an object. Allowed values: BucketOwnerFullControl, BucketOwnerRead, AuthenticatedRead, "
          "PublicReadWrite, PublicRead, Private, AwsExecRead; will be ignored if any other ACL/permission property is specified.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto UsePathStyleAccess = core::PropertyDefinitionBuilder<>::createProperty("Use Path Style Access")
      .withDescription("Path-style access can be enforced by setting this property to true. Set it to true if your endpoint does not support "
          "virtual-hosted-style requests, only path-style requests.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = minifi::utils::array_cat(S3Processor::Properties, std::array<core::PropertyReference, 10>{
      ObjectKey,
      ContentType,
      StorageClass,
      ServerSideEncryption,
      FullControlUserList,
      ReadPermissionUserList,
      ReadACLUserList,
      WriteACLUserList,
      CannedACL,
      UsePathStyleAccess
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles are routed to success relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles are routed to failure relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutS3Object(std::string name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : S3Processor(std::move(name), uuid, core::logging::LoggerFactory<PutS3Object>::getLogger(uuid)) {
  }

  ~PutS3Object() override = default;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

  class ReadCallback {
   public:
    static constexpr uint64_t MAX_SIZE = 5_GiB;
    static constexpr uint64_t BUFFER_SIZE = 4_KiB;

    ReadCallback(uint64_t flow_size, const minifi::aws::s3::PutObjectRequestParameters& options, aws::s3::S3Wrapper& s3_wrapper)
      : flow_size_(flow_size)
      , options_(options)
      , s3_wrapper_(s3_wrapper) {
    }

    int64_t operator()(const std::shared_ptr<io::InputStream>& stream) {
      if (flow_size_ > MAX_SIZE) {
        return -1;
      }
      std::vector<std::byte> buffer;
      buffer.resize(BUFFER_SIZE);
      auto data_stream = std::make_shared<std::stringstream>();
      read_size_ = 0;
      while (read_size_ < flow_size_) {
        const auto next_read_size = (std::min)(flow_size_ - read_size_, BUFFER_SIZE);
        const auto read_ret = stream->read(std::span(buffer).subspan(0, next_read_size));
        if (io::isError(read_ret)) {
          return -1;
        }
        if (read_ret > 0) {
          data_stream->write(reinterpret_cast<char*>(buffer.data()), gsl::narrow<std::streamsize>(next_read_size));
          read_size_ += read_ret;
        } else {
          break;
        }
      }
      result_ = s3_wrapper_.putObject(options_, data_stream);
      return gsl::narrow<int64_t>(read_size_);
    }

    uint64_t flow_size_;
    const minifi::aws::s3::PutObjectRequestParameters& options_;
    aws::s3::S3Wrapper& s3_wrapper_;
    uint64_t read_size_ = 0;
    std::optional<minifi::aws::s3::PutObjectResult> result_;
  };

 private:
  friend class ::S3TestsFixture<PutS3Object>;

  explicit PutS3Object(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
    : S3Processor(name, uuid, core::logging::LoggerFactory<PutS3Object>::getLogger(uuid), std::move(s3_request_sender)) {
  }

  void fillUserMetadata(const std::shared_ptr<core::ProcessContext> &context);
  static std::string parseAccessControlList(const std::string &comma_separated_list);
  bool setCannedAcl(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file, aws::s3::PutObjectRequestParameters &put_s3_request_params) const;
  bool setAccessControl(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file, aws::s3::PutObjectRequestParameters &put_s3_request_params) const;
  void setAttributes(
    const std::shared_ptr<core::ProcessSession> &session,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const aws::s3::PutObjectRequestParameters &put_s3_request_params,
    const minifi::aws::s3::PutObjectResult &put_object_result) const;
  std::optional<aws::s3::PutObjectRequestParameters> buildPutS3RequestParams(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const CommonProperties &common_properties) const;

  std::string user_metadata_;
  std::map<std::string, std::string> user_metadata_map_;
  std::string storage_class_;
  std::string server_side_encryption_;
  bool use_virtual_addressing_ = true;
};

}  // namespace org::apache::nifi::minifi::aws::processors
