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
  static const std::set<std::string> CANNED_ACLS;
  static const std::set<std::string> STORAGE_CLASSES;
  static const std::set<std::string> SERVER_SIDE_ENCRYPTIONS;

  EXTENSIONAPI static constexpr const char* Description = "This Processor puts FlowFiles to an Amazon S3 Bucket.";

  static const core::Property ObjectKey;
  static const core::Property ContentType;
  static const core::Property StorageClass;
  static const core::Property ServerSideEncryption;
  static const core::Property FullControlUserList;
  static const core::Property ReadPermissionUserList;
  static const core::Property ReadACLUserList;
  static const core::Property WriteACLUserList;
  static const core::Property CannedACL;
  static auto properties() {
    return minifi::utils::array_cat(S3Processor::properties(), std::array{
      ObjectKey,
      ContentType,
      StorageClass,
      ServerSideEncryption,
      FullControlUserList,
      ReadPermissionUserList,
      ReadACLUserList,
      WriteACLUserList,
      CannedACL
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutS3Object(std::string name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : S3Processor(std::move(name), uuid, core::logging::LoggerFactory<PutS3Object>::getLogger()) {
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
        const auto read_ret = stream->read(gsl::make_span(buffer).subspan(0, next_read_size));
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
    : S3Processor(name, uuid, core::logging::LoggerFactory<PutS3Object>::getLogger(), std::move(s3_request_sender)) {
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
};

}  // namespace org::apache::nifi::minifi::aws::processors
