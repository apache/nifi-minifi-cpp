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

#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "S3Processor.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

class ListS3 : public S3Processor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "This Processor retrieves a listing of objects from an Amazon S3 bucket.";

  EXTENSIONAPI static constexpr auto Delimiter = core::PropertyDefinitionBuilder<>::createProperty("Delimiter")
      .withDescription("The string used to delimit directories within the bucket. Please consult the AWS documentation for the correct use of this field.")
      .build();
  EXTENSIONAPI static constexpr auto Prefix = core::PropertyDefinitionBuilder<>::createProperty("Prefix")
      .withDescription("The prefix used to filter the object list. In most cases, it should end with a forward slash ('/').")
      .build();
  EXTENSIONAPI static constexpr auto UseVersions = core::PropertyDefinitionBuilder<>::createProperty("Use Versions")
      .withDescription("Specifies whether to use S3 versions, if applicable. If false, only the latest version of each object will be returned.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto MinimumObjectAge = core::PropertyDefinitionBuilder<>::createProperty("Minimum Object Age")
      .withDescription("The minimum age that an S3 object must be in order to be considered; any object younger than this amount of time (according to last modification date) will be ignored.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
      .withDefaultValue("0 sec")
      .build();
  EXTENSIONAPI static constexpr auto WriteObjectTags = core::PropertyDefinitionBuilder<>::createProperty("Write Object Tags")
      .withDescription("If set to 'true', the tags associated with the S3 object will be written as FlowFile attributes.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto WriteUserMetadata = core::PropertyDefinitionBuilder<>::createProperty("Write User Metadata")
      .isRequired(true)
      .withDescription("If set to 'true', the user defined metadata associated with the S3 object will be added to FlowFile attributes/records.")
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto RequesterPays = core::PropertyDefinitionBuilder<>::createProperty("Requester Pays")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .withDescription("If true, indicates that the requester consents to pay any charges associated with listing the S3 bucket. This sets the 'x-amz-request-payer' header to 'requester'. "
          "Note that this setting is only used if Write User Metadata is true.")
      .build();
  EXTENSIONAPI static constexpr auto Properties = minifi::utils::array_cat(S3Processor::Properties, std::to_array<core::PropertyReference>({
      Delimiter,
      Prefix,
      UseVersions,
      MinimumObjectAge,
      WriteObjectTags,
      WriteUserMetadata,
      RequesterPays
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles are routed to success relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit ListS3(std::string_view name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : S3Processor(name, uuid, core::logging::LoggerFactory<ListS3>::getLogger(uuid)) {
  }
  explicit ListS3(const std::string& name, minifi::utils::Identifier uuid, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
    : S3Processor(name, uuid, core::logging::LoggerFactory<ListS3>::getLogger(uuid), std::move(s3_request_sender)) {
  }

  ~ListS3() override = default;

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  void writeObjectTags(
    const aws::s3::ListedObjectAttributes &object_attributes,
    core::ProcessSession &session,
    core::FlowFile& flow_file);
  void writeUserMetadata(
    const aws::s3::ListedObjectAttributes &object_attributes,
    core::ProcessSession &session,
    core::FlowFile& flow_file);
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
