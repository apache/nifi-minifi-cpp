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

#include "minifi-cpp/core/Annotation.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/ProcessSessionFactory.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/controllers/RecordSetReader.h"
#include "minifi-cpp/controllers/RecordSetWriter.h"
#include "core/AbstractProcessor.h"

namespace org::apache::nifi::minifi::processors {

class SplitRecord final : public core::AbstractProcessor<SplitRecord> {
 public:
  using core::AbstractProcessor<SplitRecord>::AbstractProcessor;

  EXTENSIONAPI static constexpr const char* Description = "Splits up an input FlowFile that is in a record-oriented data format into multiple smaller FlowFiles";

  EXTENSIONAPI static constexpr auto RecordReader = core::PropertyDefinitionBuilder<>::createProperty("Record Reader")
      .withDescription("Specifies the Controller Service to use for reading incoming data")
      .isRequired(true)
      .withAllowedTypes<minifi::core::RecordSetReader>()
      .build();
  EXTENSIONAPI static constexpr auto RecordWriter = core::PropertyDefinitionBuilder<>::createProperty("Record Writer")
      .withDescription("Specifies the Controller Service to use for writing out the records")
      .isRequired(true)
      .withAllowedTypes<minifi::core::RecordSetWriter>()
      .build();
  EXTENSIONAPI static constexpr auto RecordsPerSplit = core::PropertyDefinitionBuilder<>::createProperty("Records Per Split")
      .withDescription("Specifies how many records should be written to each 'split' or 'segment' FlowFile")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      RecordReader,
      RecordWriter,
      RecordsPerSplit
  });

  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure",
      "If a FlowFile cannot be transformed from the configured input format to the configured output format, the unchanged FlowFile will be routed to this relationship."};
  EXTENSIONAPI static constexpr auto Splits = core::RelationshipDefinition{"splits",
      "The individual 'segments' of the original FlowFile will be routed to this relationship."};
  EXTENSIONAPI static constexpr auto Original = core::RelationshipDefinition{"original",
      "Upon successfully splitting an input FlowFile, the original FlowFile will be sent to this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Failure, Splits, Original};

  EXTENSIONAPI static constexpr auto RecordCount = core::OutputAttributeDefinition<1>{"record.count", {Splits},
    "The number of records in the FlowFile. This is added to FlowFiles that are routed to the 'splits' Relationship."};
  EXTENSIONAPI static constexpr auto FragmentIdentifier = core::OutputAttributeDefinition<1>{"fragment.identifier", {Splits},
    "All split FlowFiles produced from the same parent FlowFile will have this attribute set to the same UUID (which is the UUID of the parent FlowFile, if available)"};
  EXTENSIONAPI static constexpr auto FragmentIndex = core::OutputAttributeDefinition<1>{"fragment.index", {Splits},
    "A one-up number that indicates the ordering of the split FlowFiles that were created from a single parent FlowFile"};
  EXTENSIONAPI static constexpr auto FragmentCount = core::OutputAttributeDefinition<1>{"fragment.count", {Splits}, "The number of split FlowFiles generated from the parent FlowFile"};
  EXTENSIONAPI static constexpr auto SegmentOriginalFilename = core::OutputAttributeDefinition<1>{"segment.original.filename", {Splits}, "The filename of the parent FlowFile"};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 5>{RecordCount, FragmentIdentifier, FragmentIndex, FragmentCount, SegmentOriginalFilename};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  static nonstd::expected<std::size_t, std::string> readRecordsPerSplit(core::ProcessContext& context, const core::FlowFile& original_flow_file);

  std::shared_ptr<core::RecordSetReader> record_set_reader_;
  std::shared_ptr<core::RecordSetWriter> record_set_writer_;
};

}  // namespace org::apache::nifi::minifi::processors
