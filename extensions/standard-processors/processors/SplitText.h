/**
 * @file SplitText.h
 * SplitText class declaration
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
#include <string_view>
#include <utility>
#include <optional>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "utils/Export.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::processors {

struct SplitTextConfiguration {
  uint64_t line_split_count = 0;
  std::optional<uint64_t> maximum_fragment_size;
  uint64_t header_line_count = 0;
  std::optional<std::string> header_line_marker_characters;
  bool remove_trailing_new_lines = true;
};

namespace detail {

inline constexpr size_t SPLIT_TEXT_BUFFER_SIZE = 8192;

enum class StreamReadState {
  Ok,
  StreamReadError,
  EndOfStream
};

class LineReader {
 public:
  struct LineInfo {
    uint64_t offset = 0;
    uint64_t size = 0;  // size of the line including endline characters
    uint8_t endline_size = 0;  // number of characters used in the endline
    bool matches_starts_with = true;  // marks whether the line matches the starts_with filter of the readNextLine method (used in SplitText when Header Line Marker Characters is specified)

    bool operator==(const LineInfo& line_info) const = default;
  };

  explicit LineReader(const std::shared_ptr<io::InputStream>& stream);
  std::optional<LineInfo> readNextLine(const std::optional<std::string>& starts_with = std::nullopt);
  StreamReadState getState() const { return state_; }

 private:
  uint8_t getEndLineSize(size_t newline_position);
  void setLastLineInfoAttributes(uint8_t endline_size, const std::optional<std::string>& starts_with);
  bool readNextBuffer();
  std::optional<LineReader::LineInfo> finalizeLineInfo(uint8_t endline_size, const std::optional<std::string>& starts_with);

  size_t buffer_offset_ = 0;
  uint64_t current_buffer_count_ = 0;
  size_t last_read_size_ = 0;
  uint64_t read_size_ = 0;
  std::array<char, SPLIT_TEXT_BUFFER_SIZE> buffer_{};
  std::shared_ptr<io::InputStream> stream_;
  std::optional<LineInfo> last_line_info_;
  StreamReadState state_ = StreamReadState::Ok;
};

}  // namespace detail

class SplitText : public core::ProcessorImpl {
 public:
  explicit SplitText(std::string_view name,  const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Splits a text file into multiple smaller text files on line boundaries limited by maximum number of lines or total size of fragment. "
    "Each output split file will contain no more than the configured number of lines or bytes. If both Line Split Count and Maximum Fragment Size are specified, the split occurs at whichever "
    "limit is reached first. If the first line of a fragment exceeds the Maximum Fragment Size, that line will be output in a single split file which exceeds the configured maximum size limit. "
    "This component also allows one to specify that each split should include a header lines. Header lines can be computed by either specifying the amount of lines that should constitute a header "
    "or by using header marker to match against the read lines. If such match happens then the corresponding line will be treated as header. Keep in mind that upon the first failure of header "
    "marker match, no more matches will be performed and the rest of the data will be parsed as regular lines for a given split. If after computation of the header there are no more data, "
    "the resulting split will consists of only header lines.";

  EXTENSIONAPI static constexpr auto LineSplitCount = core::PropertyDefinitionBuilder<>::createProperty("Line Split Count")
      .withDescription("The number of lines that will be added to each split file, excluding header lines. A value of zero requires Maximum Fragment Size to be set, and line count will not "
        "be considered in determining splits.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MaximumFragmentSize = core::PropertyDefinitionBuilder<>::createProperty("Maximum Fragment Size")
      .withDescription("The maximum size of each split file, including header lines. NOTE: in the case where a single line exceeds this property (including headers, if applicable), "
        "that line will be output in a split of its own which exceeds this Maximum Fragment Size setting.")
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto HeaderLineCount = core::PropertyDefinitionBuilder<>::createProperty("Header Line Count")
      .withDescription("The number of lines that should be considered part of the header; the header lines will be duplicated to all split files.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("0")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto HeaderLineMarkerCharacters = core::PropertyDefinitionBuilder<>::createProperty("Header Line Marker Characters")
      .withDescription("The first character(s) on the line of the datafile which signifies a header line. This value is ignored when Header Line Count is non-zero. The first line not containing "
        "the Header Line Marker Characters and all subsequent lines are considered non-header")
      .build();
  EXTENSIONAPI static constexpr auto RemoveTrailingNewlines = core::PropertyDefinitionBuilder<>::createProperty("Remove Trailing Newlines")
      .withDescription("Whether to remove newlines at the end of each split file. This should be false if you intend to merge the split files later. If this is set to 'true' and a FlowFile is "
        "generated that contains only 'empty lines' (i.e., consists only of \\r and \\n characters), the FlowFile will not be emitted. Note, however, that if header lines are specified, "
        "the resultant FlowFile will never be empty as it will consist of the header lines, so a FlowFile may be emitted that contains only the header lines.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      LineSplitCount,
      MaximumFragmentSize,
      HeaderLineCount,
      HeaderLineMarkerCharacters,
      RemoveTrailingNewlines
  });

  EXTENSIONAPI static constexpr auto Failure =
    core::RelationshipDefinition{"failure", "If a file cannot be split for some reason, the original file will be routed to this destination and nothing will be routed elsewhere"};
  EXTENSIONAPI static constexpr auto Original =
    core::RelationshipDefinition{"original", "The original input file will be routed to this destination when it has been successfully split into 1 or more files"};
  EXTENSIONAPI static constexpr auto Splits =
    core::RelationshipDefinition{"splits", "The split files will be routed to this destination when an input file is successfully split into 1 or more split files"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Failure, Original, Splits};

  EXTENSIONAPI static constexpr auto TextLineCountOutputAttribute =
    core::OutputAttributeDefinition<0>{"text.line.count", {}, "The number of lines of text from the original FlowFile that were copied to this FlowFile (does not count empty lines)"};
  EXTENSIONAPI static constexpr auto FragmentSizeOutputAttribute =
    core::OutputAttributeDefinition<0>{"fragment.size", {},
      "The number of bytes from the original FlowFile that were copied to this FlowFile, including header, if applicable, which is duplicated in each split FlowFile"};
  EXTENSIONAPI static constexpr auto FragmentIdentifierOutputAttribute =
    core::OutputAttributeDefinition<0>{"fragment.identifier", {}, "All split FlowFiles produced from the same parent FlowFile will have the same randomly generated UUID added for this attribute"};
  EXTENSIONAPI static constexpr auto FragmentIndexOutputAttribute =
    core::OutputAttributeDefinition<0>{"fragment.index", {}, "A one-up number that indicates the ordering of the split FlowFiles that were created from a single parent FlowFile"};
  EXTENSIONAPI static constexpr auto FragmentCountOutputAttribute =
    core::OutputAttributeDefinition<0>{"fragment.count", {}, "The number of split FlowFiles generated from the parent FlowFile"};
  EXTENSIONAPI static constexpr auto SegmentOriginalFilenameOutputAttribute =
    core::OutputAttributeDefinition<0>{"segment.original.filename", {}, "The filename of the parent FlowFile"};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 6>{TextLineCountOutputAttribute, FragmentSizeOutputAttribute, FragmentIdentifierOutputAttribute,
    FragmentIndexOutputAttribute, FragmentCountOutputAttribute, SegmentOriginalFilenameOutputAttribute};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  SplitTextConfiguration split_text_config_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<SplitText>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
