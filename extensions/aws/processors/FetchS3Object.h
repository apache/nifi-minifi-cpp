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

#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "io/StreamPipe.h"
#include "S3Processor.h"
#include "utils/ArrayUtils.h"
#include "utils/GeneralUtils.h"

template<typename T>
class FlowProcessorS3TestsFixture;

namespace org::apache::nifi::minifi::aws::processors {

class FetchS3Object : public S3Processor {  // NOLINT(cppcoreguidelines-special-member-functions)
 public:
  EXTENSIONAPI static constexpr const char* Description = "This Processor retrieves the contents of an S3 Object and writes it to the content of a FlowFile.";

  EXTENSIONAPI static constexpr auto ObjectKey = core::PropertyDefinitionBuilder<>::createProperty("Object Key")
      .withDescription("The key of the S3 object. If none is given the filename attribute will be used by default.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Version = core::PropertyDefinitionBuilder<>::createProperty("Version")
      .withDescription("The Version of the Object to download")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto RequesterPays = core::PropertyDefinitionBuilder<>::createProperty("Requester Pays")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .withDescription("If true, indicates that the requester consents to pay any charges associated with retrieving "
          "objects from the S3 bucket. This sets the 'x-amz-request-payer' header to 'requester'.")
      .build();
  EXTENSIONAPI static constexpr auto Properties = minifi::utils::array_cat(S3Processor::Properties, std::to_array<core::PropertyReference>({
      ObjectKey,
      Version,
      RequesterPays
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles are routed to success relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles are routed to failure relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr auto S3Etag = core::OutputAttributeDefinition<>{
      "s3.etag", {Success}, "The ETag that can be used to see if the file has changed"};
  EXTENSIONAPI static constexpr auto S3ExpirationTime = core::OutputAttributeDefinition<>{
      "s3.expirationTime", {Success}, "The expiration time of the S3 object"};
  EXTENSIONAPI static constexpr auto S3ExpirationTimeRuleId = core::OutputAttributeDefinition<>{
      "s3.expirationTimeRuleId", {Success}, "The ID of the rule that dictates this object's expiration time"};
  EXTENSIONAPI static constexpr auto S3SseAlgorithm = core::OutputAttributeDefinition<>{
      "s3.sseAlgorithm", {Success}, "The server side encryption algorithm of the object"};
  EXTENSIONAPI static constexpr auto S3Version = core::OutputAttributeDefinition<>{
      "s3.version", {Success}, "The version of the S3 Object that was put to S3"};
  EXTENSIONAPI static constexpr auto S3Exception = core::OutputAttributeDefinition<>{
      S3_EXCEPTION, {Failure}, "Exception name of the S3 request failure"};
  EXTENSIONAPI static constexpr auto S3ErrorMessage = core::OutputAttributeDefinition<>{
      S3_ERROR_MESSAGE, {Failure}, "Error message of the S3 request failure"};
  EXTENSIONAPI static constexpr auto S3ErrorRetryable = core::OutputAttributeDefinition<>{
      S3_ERROR_RETRYABLE, {Failure}, "Is the S3 request error retryable"};
  EXTENSIONAPI static constexpr auto S3StatusCode = core::OutputAttributeDefinition<>{
      S3_STATUS_CODE, {Failure}, "HTTP code of the S3 request failure. -1 indicates no HTTP request was made."};
  EXTENSIONAPI static constexpr auto OutputAttributes =
      std::to_array<core::OutputAttributeReference>({S3Etag, S3ExpirationTime, S3ExpirationTimeRuleId, S3SseAlgorithm, S3Version, S3Exception, S3ErrorMessage, S3ErrorRetryable, S3StatusCode});

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  using S3Processor::S3Processor;

  ~FetchS3Object() override = default;

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  friend class ::FlowProcessorS3TestsFixture<FetchS3Object>;

  FetchS3Object(core::ProcessorMetadata metadata, S3WrapperFactory s3_wrapper_factory)
      : S3Processor(std::move(metadata), std::move(s3_wrapper_factory)) {
  }

  std::optional<aws::s3::GetObjectRequestParameters> buildFetchS3RequestParams(
    const core::ProcessContext& context,
    const core::FlowFile& flow_file,
    std::string_view bucket) const;

  bool requester_pays_ = false;
};

}  // namespace org::apache::nifi::minifi::aws::processors
