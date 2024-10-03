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

#include "FlowFileRecord.h"
#include "core/ProcessSession.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class SplitContent final : public core::Processor {
 public:
  explicit SplitContent(const std::string_view name, const utils::Identifier& uuid = {}) : Processor(name, uuid) {}

  enum class ByteSequenceFormat { Hexadecimal, Text };

  enum class ByteSequenceLocation { Trailing, Leading };

  EXTENSIONAPI static constexpr auto Description = "Splits incoming FlowFiles by a specified byte sequence";

  EXTENSIONAPI static constexpr auto ByteSequenceFormatProperty =
      core::PropertyDefinitionBuilder<2>::createProperty("Byte Sequence Format")
          .withDescription("Specifies how the <Byte Sequence> property should be interpreted")
          .isRequired(true)
          .withDefaultValue(magic_enum::enum_name(ByteSequenceFormat::Hexadecimal))
          .withAllowedValues(magic_enum::enum_names<ByteSequenceFormat>())
          .build();

  EXTENSIONAPI static constexpr auto ByteSequence =
      core::PropertyDefinitionBuilder<>::createProperty("Byte Sequence")
          .withDescription("A representation of bytes to look for and upon which to split the source file into separate files")
          .isRequired(true)
          .withPropertyType(core::StandardPropertyTypes::NON_BLANK_TYPE)
          .build();

  EXTENSIONAPI static constexpr auto KeepByteSequence =
      core::PropertyDefinitionBuilder<>::createProperty("Keep Byte Sequence")
          .withDescription("Determines whether or not the Byte Sequence should be included with each Split")
          .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
          .withDefaultValue("false")
          .isRequired(true)
          .build();

  EXTENSIONAPI static constexpr auto ByteSequenceLocationProperty =
      core::PropertyDefinitionBuilder<2>::createProperty("Byte Sequence Location")
          .withDescription(
              "If <Keep Byte Sequence> is set to true, specifies whether the byte sequence should be added to the end of the first split or the beginning of the second; "
              "if <Keep Byte Sequence> is false, this property is ignored.")
          .withDefaultValue(magic_enum::enum_name(ByteSequenceLocation::Trailing))
          .withAllowedValues(magic_enum::enum_names<ByteSequenceLocation>())
          .isRequired(true)
          .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({ByteSequenceFormatProperty, ByteSequence, KeepByteSequence, ByteSequenceLocationProperty});

  EXTENSIONAPI static constexpr auto Splits = core::RelationshipDefinition{"splits", "All Splits will be routed to the splits relationship"};
  EXTENSIONAPI static constexpr auto Original = core::RelationshipDefinition{"original", "The original file"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Original, Splits};

  EXTENSIONAPI static constexpr auto FragmentIdentifierOutputAttribute =
      core::OutputAttributeDefinition<0>{"fragment.identifier", {}, "All split FlowFiles produced from the same parent FlowFile will have the same randomly generated UUID added for this attribute"};
  EXTENSIONAPI static constexpr auto FragmentIndexOutputAttribute =
      core::OutputAttributeDefinition<0>{"fragment.index", {}, "A one-up number that indicates the ordering of the split FlowFiles that were created from a single parent FlowFile"};
  EXTENSIONAPI static constexpr auto FragmentCountOutputAttribute = core::OutputAttributeDefinition<0>{"fragment.count", {}, "The number of split FlowFiles generated from the parent FlowFile"};
  EXTENSIONAPI static constexpr auto SegmentOriginalFilenameOutputAttribute = core::OutputAttributeDefinition<0>{"segment.original.filename", {}, "The filename of the parent FlowFile"};
  EXTENSIONAPI static constexpr auto OutputAttributes =
      std::array<core::OutputAttributeReference, 4>{FragmentIdentifierOutputAttribute, FragmentIndexOutputAttribute, FragmentCountOutputAttribute, SegmentOriginalFilenameOutputAttribute};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr auto InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  std::shared_ptr<core::FlowFile> createNewSplit(core::ProcessSession& session) const;
  void finalizeLatestSplitContent(core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& latest_split, const std::vector<std::byte>& buffer) const;
  void finalizeLastSplitContent(core::ProcessSession& session, std::vector<std::shared_ptr<core::FlowFile>>& splits, const std::vector<std::byte>& buffer, bool ended_with_byte_sequence) const;

  std::vector<std::byte> byte_sequence_{};
  bool keep_byte_sequence = false;
  ByteSequenceLocation byte_sequence_location_ = ByteSequenceLocation::Trailing;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<SplitContent>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
