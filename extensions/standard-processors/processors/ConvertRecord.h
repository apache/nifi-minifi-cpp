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
#include <string_view>
#include <utility>
#include <optional>

#include "core/AbstractProcessor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "minifi-cpp/controllers/RecordConverter.h"

namespace org::apache::nifi::minifi::processors {

class ConvertRecord : public core::AbstractProcessor<ConvertRecord> {
 public:
  using core::AbstractProcessor<ConvertRecord>::AbstractProcessor;

  EXTENSIONAPI static constexpr const char* Description = "Converts records from one data format to another using configured Record Reader and Record Writer Controller Services.";

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
  EXTENSIONAPI static constexpr auto IncludeZeroRecordFlowFiles = core::PropertyDefinitionBuilder<>::createProperty("Include Zero Record FlowFiles")
      .withDescription("When converting an incoming FlowFile, if the conversion results in no data, this property specifies whether or not a FlowFile will be sent to the corresponding relationship.")
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("true")
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      RecordReader,
      RecordWriter,
      IncludeZeroRecordFlowFiles
  });

  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure",
    "If a FlowFile cannot be transformed from the configured input format to the configured output format, the unchanged FlowFile will be routed to this relationship"};
  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles that are successfully transformed will be routed to this relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Failure, Success};

  EXTENSIONAPI static constexpr auto RecordCountOutputAttribute = core::OutputAttributeDefinition<1>{"record.count", {Success}, "The number of records in the FlowFile"};
  EXTENSIONAPI static constexpr auto RecordErrorMessageOutputAttribute = core::OutputAttributeDefinition<1>{"record.error.message", {Failure},
      "This attribute provides on failure the error message encountered by the Reader or Writer."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 2>{RecordCountOutputAttribute, RecordErrorMessageOutputAttribute};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::optional<core::RecordConverter> record_converter_;
  bool include_zero_record_flow_files_ = true;
};

}  // namespace org::apache::nifi::minifi::processors
