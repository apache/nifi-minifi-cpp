/**
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
#include <string_view>
#include <utility>

#include "minifi-cpp/FlowFileRecord.h"
#include "core/ProcessSession.h"
#include "core/ProcessorImpl.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "minifi-cpp/utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class SegmentContent final : public core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  EXTENSIONAPI static constexpr auto Description = "Segments a FlowFile into multiple smaller segments on byte boundaries.";

  EXTENSIONAPI static constexpr auto SegmentSize =
      core::PropertyDefinitionBuilder<>::createProperty("Segment Size")
        .withDescription("The maximum data size in bytes for each segment")
        .isRequired(true)
        .supportsExpressionLanguage(true)
        .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({SegmentSize});

  EXTENSIONAPI static constexpr auto Segments = core::RelationshipDefinition{
      "segments", "All segments will be sent to this relationship. If the file was small enough that it was not segmented, a copy of the original is sent to this relationship as well as original"};
  EXTENSIONAPI static constexpr auto Original = core::RelationshipDefinition{"original", "The original FlowFile will be sent to this relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Original, Segments};

  EXTENSIONAPI static constexpr auto FragmentIdentifierOutputAttribute =
      core::OutputAttributeDefinition<0>{"fragment.identifier", {},
            "All segments produced from the same parent FlowFile will have this attribute set to the same UUID (which is the UUID of the parent FlowFile, if available)"};
  EXTENSIONAPI static constexpr auto FragmentIndexOutputAttribute =
      core::OutputAttributeDefinition<0>{"fragment.index", {}, "A sequence number starting with 1 that indicates the ordering of the segments that were created from a single parent FlowFile"};
  EXTENSIONAPI static constexpr auto FragmentCountOutputAttribute = core::OutputAttributeDefinition<0>{"fragment.count", {}, "The number of segments generated from the parent FlowFile"};
  EXTENSIONAPI static constexpr auto SegmentOriginalFilenameOutputAttribute = core::OutputAttributeDefinition<0>{"segment.original.filename", {}, "The filename of the parent FlowFile"};
  EXTENSIONAPI static constexpr auto OutputAttributes =
      std::to_array<core::OutputAttributeReference>({FragmentIdentifierOutputAttribute, FragmentIndexOutputAttribute, FragmentCountOutputAttribute, SegmentOriginalFilenameOutputAttribute});

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr auto InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  size_t buffer_size_{};
};

}  // namespace org::apache::nifi::minifi::processors
