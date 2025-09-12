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
#include <sstream>
#include <string>
#include <utility>

#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "S3Processor.h"
#include "utils/ArrayUtils.h"
#include "utils/GeneralUtils.h"


template<typename T>
class FlowProcessorS3TestsFixture;

namespace org::apache::nifi::minifi::aws::processors {

class DeleteS3Object : public S3Processor {  // NOLINT(cppcoreguidelines-special-member-functions)
 public:
  EXTENSIONAPI static constexpr const char* Description = "Deletes FlowFiles on an Amazon S3 Bucket. If attempting to delete a file that does not exist, FlowFile is routed to success.";

  EXTENSIONAPI static constexpr auto ObjectKey = core::PropertyDefinitionBuilder<>::createProperty("Object Key")
    .withDescription("The key of the S3 object. If none is given the filename attribute will be used by default.")
    .supportsExpressionLanguage(true)
    .build();
  EXTENSIONAPI static constexpr auto Version = core::PropertyDefinitionBuilder<>::createProperty("Version")
    .withDescription("The Version of the Object to delete")
    .supportsExpressionLanguage(true)
    .build();
  EXTENSIONAPI static constexpr auto Properties = minifi::utils::array_cat(S3Processor::Properties, std::to_array<core::PropertyReference>({
      ObjectKey,
      Version
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles are routed to success relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles are routed to failure relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  using S3Processor::S3Processor;

  ~DeleteS3Object() override = default;

  void initialize() override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  friend class ::FlowProcessorS3TestsFixture<DeleteS3Object>;

  explicit DeleteS3Object(core::ProcessorMetadata metadata, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
    : S3Processor(metadata, std::move(s3_request_sender)) {
  }

  std::optional<aws::s3::DeleteObjectRequestParameters> buildDeleteS3RequestParams(
    const core::ProcessContext& context,
    const core::FlowFile& flow_file,
    const CommonProperties &common_properties,
    std::string_view bucket) const;
};

}  // namespace org::apache::nifi::minifi::aws::processors
