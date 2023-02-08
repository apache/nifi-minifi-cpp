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

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <tuple>
#include <memory>

#include "S3Processor.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

class ListS3 : public S3Processor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "This Processor retrieves a listing of objects from an Amazon S3 bucket.";

  EXTENSIONAPI static const core::Property Delimiter;
  EXTENSIONAPI static const core::Property Prefix;
  EXTENSIONAPI static const core::Property UseVersions;
  EXTENSIONAPI static const core::Property MinimumObjectAge;
  EXTENSIONAPI static const core::Property WriteObjectTags;
  EXTENSIONAPI static const core::Property WriteUserMetadata;
  EXTENSIONAPI static const core::Property RequesterPays;
  static auto properties() {
    return minifi::utils::array_cat(S3Processor::properties(), std::array{
      Delimiter,
      Prefix,
      UseVersions,
      MinimumObjectAge,
      WriteObjectTags,
      WriteUserMetadata,
      RequesterPays
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit ListS3(std::string name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : S3Processor(std::move(name), uuid, core::logging::LoggerFactory<ListS3>::getLogger(uuid)) {
  }
  explicit ListS3(const std::string& name, minifi::utils::Identifier uuid, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
    : S3Processor(name, uuid, core::logging::LoggerFactory<ListS3>::getLogger(uuid), std::move(s3_request_sender)) {
  }

  ~ListS3() override = default;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  void writeObjectTags(
    const aws::s3::ListedObjectAttributes &object_attributes,
    core::ProcessSession &session,
    const std::shared_ptr<core::FlowFile> &flow_file);
  void writeUserMetadata(
    const aws::s3::ListedObjectAttributes &object_attributes,
    core::ProcessSession &session,
    const std::shared_ptr<core::FlowFile> &flow_file);
  void createNewFlowFile(
    core::ProcessSession &session,
    const aws::s3::ListedObjectAttributes &object_attributes);

  std::unique_ptr<aws::s3::ListRequestParameters> list_request_params_;
  bool write_object_tags_ = false;
  bool write_user_metadata_ = false;
  bool requester_pays_ = false;
  std::unique_ptr<minifi::utils::ListingStateManager> state_manager_;
};

}  // namespace org::apache::nifi::minifi::aws::processors
