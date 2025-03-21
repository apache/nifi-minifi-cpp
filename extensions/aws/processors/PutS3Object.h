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
#include <chrono>

#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "io/StreamPipe.h"
#include "S3Processor.h"
#include "utils/ArrayUtils.h"
#include "utils/gsl.h"
#include "utils/Id.h"
#include "utils/Enum.h"

template<typename T>
class FlowProcessorS3TestsFixture;

namespace org::apache::nifi::minifi::aws::processors {

class PutS3Object : public S3Processor {
 public:
  static constexpr auto CANNED_ACLS = minifi::utils::getKeys(minifi::aws::s3::CANNED_ACL_MAP);
  static constexpr auto STORAGE_CLASSES = minifi::utils::getKeys(minifi::aws::s3::STORAGE_CLASS_MAP);
  static constexpr auto SERVER_SIDE_ENCRYPTIONS = minifi::utils::getKeys(minifi::aws::s3::SERVER_SIDE_ENCRYPTION_MAP);
  static constexpr auto CHECKSUM_ALGORITHMS = minifi::utils::getKeys(minifi::aws::s3::CHECKSUM_ALGORITHM_MAP);

  EXTENSIONAPI static constexpr const char* Description = "Puts FlowFiles to an Amazon S3 Bucket. The upload uses either the PutS3Object method or the PutS3MultipartUpload method. "
      "The PutS3Object method sends the file in a single synchronous call, but it has a 5GB size limit. Larger files are sent using the PutS3MultipartUpload method. "
      "This multipart process saves state after each step so that a large upload can be resumed with minimal loss if the processor or cluster is stopped and restarted. "
      "A multipart upload consists of three steps: 1) initiate upload, 2) upload the parts, and 3) complete the upload. For multipart uploads, the processor saves state "
      "locally tracking the upload ID and parts uploaded, which must both be provided to complete the upload. The AWS libraries select an endpoint URL based on the AWS region, "
      "but this can be overridden with the 'Endpoint Override URL' property for use with other S3-compatible endpoints. The S3 API specifies that the maximum file size for a "
      "PutS3Object upload is 5GB. It also requires that parts in a multipart upload must be at least 5MB in size, except for the last part. These limits establish the bounds "
      "for the Multipart Upload Threshold and Part Size properties.";

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
  EXTENSIONAPI static constexpr auto MultipartThreshold = core::PropertyDefinitionBuilder<>::createProperty("Multipart Threshold")
      .withDescription("Specifies the file size threshold for switch from the PutS3Object API to the PutS3MultipartUpload API. "
                        "Flow files bigger than this limit will be sent using the multipart process. The valid range is 5MB to 5GB.")
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("5 GB")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MultipartPartSize = core::PropertyDefinitionBuilder<>::createProperty("Multipart Part Size")
      .withDescription("Specifies the part size for use when the PutS3Multipart Upload API is used. "
                        "Flow files will be broken into chunks of this size for the upload process, but the last part sent can be smaller since it is not padded. The valid range is 5MB to 5GB.")
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("5 GB")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MultipartUploadAgeOffInterval = core::PropertyDefinitionBuilder<>::createProperty("Multipart Upload AgeOff Interval")
      .withDescription("Specifies the interval at which existing multipart uploads in AWS S3 will be evaluated for ageoff. "
                        "When processor is triggered it will initiate the ageoff evaluation if this interval has been exceeded.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("60 min")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MultipartUploadMaxAgeThreshold = core::PropertyDefinitionBuilder<>::createProperty("Multipart Upload Max Age Threshold")
      .withDescription("Specifies the maximum age for existing multipart uploads in AWS S3. When the ageoff process occurs, any upload older than this threshold will be aborted.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("7 days")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto ChecksumAlgorithm = core::PropertyDefinitionBuilder<CHECKSUM_ALGORITHMS.size()>::createProperty("Checksum Algorithm")
      .withDescription("Checksum algorithm used to verify the uploaded object.")
      .withAllowedValues(CHECKSUM_ALGORITHMS)
      .withDefaultValue("CRC64NVME")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = minifi::utils::array_cat(S3Processor::Properties, std::to_array<core::PropertyReference>({
      ObjectKey,
      ContentType,
      StorageClass,
      ServerSideEncryption,
      FullControlUserList,
      ReadPermissionUserList,
      ReadACLUserList,
      WriteACLUserList,
      CannedACL,
      UsePathStyleAccess,
      MultipartThreshold,
      MultipartPartSize,
      MultipartUploadAgeOffInterval,
      MultipartUploadMaxAgeThreshold,
      ChecksumAlgorithm
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles are routed to success relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles are routed to failure relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutS3Object(std::string_view name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : S3Processor(name, uuid, core::logging::LoggerFactory<PutS3Object>::getLogger(uuid)) {
  }

  ~PutS3Object() override = default;

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 protected:
  static constexpr uint64_t MIN_PART_SIZE = 5_MiB;
  static constexpr uint64_t MAX_UPLOAD_SIZE = 5_GiB;

  friend class ::FlowProcessorS3TestsFixture<PutS3Object>;

  explicit PutS3Object(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
    : S3Processor(name, uuid, core::logging::LoggerFactory<PutS3Object>::getLogger(uuid), std::move(s3_request_sender)) {
  }

  virtual uint64_t getMinPartSize() const {
    return MIN_PART_SIZE;
  }

  virtual uint64_t getMaxUploadSize() const {
    return MAX_UPLOAD_SIZE;
  }

  void fillUserMetadata(core::ProcessContext& context);
  static std::string parseAccessControlList(const std::string &comma_separated_list);
  bool setCannedAcl(core::ProcessContext& context, const core::FlowFile& flow_file, aws::s3::PutObjectRequestParameters &put_s3_request_params) const;
  bool setAccessControl(core::ProcessContext& context, const core::FlowFile& flow_file, aws::s3::PutObjectRequestParameters &put_s3_request_params) const;
  void setAttributes(
    core::ProcessSession& session,
    core::FlowFile& flow_file,
    const aws::s3::PutObjectRequestParameters &put_s3_request_params,
    const minifi::aws::s3::PutObjectResult &put_object_result) const;
  std::optional<aws::s3::PutObjectRequestParameters> buildPutS3RequestParams(
    core::ProcessContext& context,
    const core::FlowFile& flow_file,
    const CommonProperties &common_properties) const;
  void ageOffMultipartUploads(const CommonProperties &common_properties);

  std::string user_metadata_;
  std::map<std::string, std::string> user_metadata_map_;
  std::string storage_class_;
  std::string server_side_encryption_;
  bool use_virtual_addressing_ = true;
  uint64_t multipart_threshold_{};
  uint64_t multipart_size_{};
  std::chrono::milliseconds multipart_upload_ageoff_interval_;
  std::chrono::milliseconds multipart_upload_max_age_threshold_;
  std::mutex last_ageoff_mutex_;
  std::chrono::time_point<std::chrono::system_clock> last_ageoff_time_;
  Aws::S3::Model::ChecksumAlgorithm checksum_algorithm_{Aws::S3::Model::ChecksumAlgorithm::CRC64NVME};
};

}  // namespace org::apache::nifi::minifi::aws::processors
