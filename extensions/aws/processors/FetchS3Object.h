/**
 * @file FetchS3Object.h
 * FetchS3Object class declaration
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
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include "S3Processor.h"
#include "utils/GeneralUtils.h"

template<typename T>
class S3TestsFixture;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

class FetchS3Object : public S3Processor {
 public:
  static constexpr char const* ProcessorName = "FetchS3Object";

  // Supported Properties
  static const core::Property ObjectKey;
  static const core::Property Version;
  static const core::Property RequesterPays;

  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  explicit FetchS3Object(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : S3Processor(name, uuid, logging::LoggerFactory<FetchS3Object>::getLogger()) {
  }

  ~FetchS3Object() override = default;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(uint64_t flow_size, const minifi::aws::s3::GetObjectRequestParameters& get_object_params, aws::s3::S3Wrapper& s3_wrapper)
      : flow_size_(flow_size)
      , get_object_params_(get_object_params)
      , s3_wrapper_(s3_wrapper) {
    }

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      std::vector<uint8_t> buffer;
      result_ = s3_wrapper_.getObject(get_object_params_, *stream);
      if (!result_) {
        return 0;
      }

      return result_->write_size;
    }

    uint64_t flow_size_;

    const minifi::aws::s3::GetObjectRequestParameters& get_object_params_;
    aws::s3::S3Wrapper& s3_wrapper_;
    uint64_t write_size_ = 0;
    std::optional<minifi::aws::s3::GetObjectResult> result_;
  };

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  friend class ::S3TestsFixture<FetchS3Object>;

  explicit FetchS3Object(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
    : S3Processor(name, uuid, logging::LoggerFactory<FetchS3Object>::getLogger(), std::move(s3_request_sender)) {
  }

  std::optional<aws::s3::GetObjectRequestParameters> buildFetchS3RequestParams(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const CommonProperties &common_properties) const;

  bool requester_pays_ = false;
};

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
