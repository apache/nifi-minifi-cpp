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

#include <memory>
#include <optional>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include "io/StreamPipe.h"
#include "S3Processor.h"
#include "utils/ArrayUtils.h"
#include "utils/GeneralUtils.h"

template<typename T>
class S3TestsFixture;

namespace org::apache::nifi::minifi::aws::processors {

class FetchS3Object : public S3Processor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "This Processor retrieves the contents of an S3 Object and writes it to the content of a FlowFile.";

  static const core::Property ObjectKey;
  static const core::Property Version;
  static const core::Property RequesterPays;
  static auto properties() {
    return minifi::utils::array_cat(S3Processor::properties(), std::array{
      ObjectKey,
      Version,
      RequesterPays
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

  explicit FetchS3Object(std::string name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : S3Processor(std::move(name), uuid, core::logging::LoggerFactory<FetchS3Object>::getLogger()) {
  }

  ~FetchS3Object() override = default;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::S3TestsFixture<FetchS3Object>;

  explicit FetchS3Object(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
    : S3Processor(name, uuid, core::logging::LoggerFactory<FetchS3Object>::getLogger(), std::move(s3_request_sender)) {
  }

  std::optional<aws::s3::GetObjectRequestParameters> buildFetchS3RequestParams(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const CommonProperties &common_properties) const;

  bool requester_pays_ = false;
};

}  // namespace org::apache::nifi::minifi::aws::processors
