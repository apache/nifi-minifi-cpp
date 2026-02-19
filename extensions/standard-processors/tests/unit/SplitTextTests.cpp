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

#include "minifi-cpp/FlowFileRecord.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "processors/SplitText.h"
#include "unit/SingleProcessorTestController.h"
#include "io/BufferStream.h"
#include "utils/ConfigurationUtils.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

inline constexpr auto BUFFER_SIZE = minifi::utils::configuration::DEFAULT_BUFFER_SIZE;

TEST_CASE("Test LineReader with nullptr") {
  processors::detail::LineReader reader{nullptr, BUFFER_SIZE};
  CHECK(reader.readNextLine() == std::nullopt);
  CHECK(reader.getState() == processors::detail::StreamReadState::EndOfStream);
}

TEST_CASE("Test LineReader with empty stream") {
  auto stream = std::make_shared<io::BufferStream>();
  processors::detail::LineReader reader{stream, BUFFER_SIZE};
  CHECK(reader.readNextLine() == std::nullopt);
  CHECK(reader.getState() == processors::detail::StreamReadState::EndOfStream);
}

TEST_CASE("Test LineReader with trailing endline") {
  auto stream = std::make_shared<io::BufferStream>();
  std::string input = "this is a new line\nand another line\r\nthirdline\n";
  stream->write(reinterpret_cast<const uint8_t*>(input.data()), input.size());
  processors::detail::LineReader reader{stream, BUFFER_SIZE};
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = 0, .size = 19, .endline_size = 1});
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = 19, .size = 18, .endline_size = 2});
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = 37, .size = 10, .endline_size = 1});
  CHECK(reader.readNextLine() == std::nullopt);
  CHECK(reader.getState() == processors::detail::StreamReadState::EndOfStream);
}

TEST_CASE("Test LineReader without trailing endlines") {
  auto stream = std::make_shared<io::BufferStream>();
  std::string input = "this is a new line\nand another line\r\nthirdline";
  stream->write(reinterpret_cast<const uint8_t*>(input.data()), input.size());
  processors::detail::LineReader reader{stream, BUFFER_SIZE};
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = 0, .size = 19, .endline_size = 1});
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = 19, .size = 18, .endline_size = 2});
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = 37, .size = 9, .endline_size = 0});
  CHECK(reader.readNextLine() == std::nullopt);
  CHECK(reader.getState() == processors::detail::StreamReadState::EndOfStream);
}

TEST_CASE("Test LineReader with input larger than buffer length") {
  auto stream = std::make_shared<io::BufferStream>();
  const auto first_line_size = static_cast<size_t>(BUFFER_SIZE * 1.5);
  const auto second_line_size = static_cast<size_t>(BUFFER_SIZE * 1.7);
  std::string input = std::string(first_line_size, 'a') + "\n" + std::string(second_line_size, 'b') + "\n";
  stream->write(reinterpret_cast<const uint8_t*>(input.data()), input.size());
  processors::detail::LineReader reader{stream, BUFFER_SIZE};
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = 0, .size = first_line_size + 1, .endline_size = 1});
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = first_line_size +1 , .size = second_line_size + 1, .endline_size = 1});
  CHECK(reader.readNextLine() == std::nullopt);
  CHECK(reader.getState() == processors::detail::StreamReadState::EndOfStream);
}

TEST_CASE("Test LineReader with input of same size as buffer length") {
  auto stream = std::make_shared<io::BufferStream>();
  std::string input = std::string(BUFFER_SIZE - 1, 'a') + "\n" + std::string(BUFFER_SIZE * 2 - 1, 'b') + "\n";
  stream->write(reinterpret_cast<const uint8_t*>(input.data()), input.size());
  processors::detail::LineReader reader{stream, BUFFER_SIZE};
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = 0, .size = BUFFER_SIZE, .endline_size = 1});
  CHECK(reader.readNextLine() ==
    processors::detail::LineReader::LineInfo{.offset = BUFFER_SIZE, .size = BUFFER_SIZE * 2, .endline_size = 1});
  CHECK(reader.readNextLine() == std::nullopt);
  CHECK(reader.getState() == processors::detail::StreamReadState::EndOfStream);
}

TEST_CASE("Test LineReader with input larger than buffer length without trailing endline") {
  auto stream = std::make_shared<io::BufferStream>();
  const auto first_line_size = static_cast<size_t>(BUFFER_SIZE * 1.5);
  const auto second_line_size = static_cast<size_t>(BUFFER_SIZE * 1.7);
  std::string input = std::string(first_line_size, 'a') + "\n" + std::string(second_line_size, 'b');
  stream->write(reinterpret_cast<const uint8_t*>(input.data()), input.size());
  processors::detail::LineReader reader{stream, BUFFER_SIZE};
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = 0, .size = first_line_size + 1, .endline_size = 1});
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = first_line_size + 1, .size = second_line_size, .endline_size = 0});
  CHECK(reader.readNextLine() == std::nullopt);
  CHECK(reader.getState() == processors::detail::StreamReadState::EndOfStream);
}

TEST_CASE("Test LineReader with input of same size as buffer length without trailing endline") {
  auto stream = std::make_shared<io::BufferStream>();
  std::string input = std::string(BUFFER_SIZE - 1, 'a') + "\n" + std::string(BUFFER_SIZE * 2, 'b');
  stream->write(reinterpret_cast<const uint8_t*>(input.data()), input.size());
  processors::detail::LineReader reader{stream, BUFFER_SIZE};
  CHECK(reader.readNextLine() == processors::detail::LineReader::LineInfo{.offset = 0, .size = BUFFER_SIZE, .endline_size = 1});
  CHECK(reader.readNextLine() ==
    processors::detail::LineReader::LineInfo{.offset = BUFFER_SIZE, .size = BUFFER_SIZE * 2, .endline_size = 0});
  CHECK(reader.readNextLine() == std::nullopt);
  CHECK(reader.getState() == processors::detail::StreamReadState::EndOfStream);
}

TEST_CASE("Test LineReader with starts with filter") {
  auto stream = std::make_shared<io::BufferStream>();
  std::string input = "header this is a new line\nheader and another line\r\nthirdline\nheader line\n";
  stream->write(reinterpret_cast<const uint8_t*>(input.data()), input.size());
  processors::detail::LineReader reader{stream, BUFFER_SIZE};
  CHECK(reader.readNextLine("header") == processors::detail::LineReader::LineInfo{.offset = 0, .size = 26, .endline_size = 1, .matches_starts_with = true});
  CHECK(reader.readNextLine("header") == processors::detail::LineReader::LineInfo{.offset = 26, .size = 25, .endline_size = 2, .matches_starts_with = true});
  CHECK(reader.readNextLine("header") == processors::detail::LineReader::LineInfo{.offset = 51, .size = 10, .endline_size = 1, .matches_starts_with = false});
  CHECK(reader.readNextLine("header") == processors::detail::LineReader::LineInfo{.offset = 61, .size = 12, .endline_size = 1, .matches_starts_with = true});
  CHECK(reader.readNextLine() == std::nullopt);
  CHECK(reader.getState() == processors::detail::StreamReadState::EndOfStream);
}

struct ExpectedSplitTextResult {
  std::string content;
  uint64_t fragment_index = 0;
  uint64_t fragment_count = 0;
  uint64_t text_line_count = 0;
};

struct SplitTextProperties {
  uint64_t line_split_count = 0;
  std::optional<bool> trim_trailing_newlines;
  std::optional<uint64_t> maximum_fragment_size;
  std::optional<uint64_t> header_line_count;
  std::optional<std::string> header_line_marker_characters;
};

void verifySplitResults(const SingleProcessorTestController& controller, const ProcessorTriggerResult& trigger_results, const std::vector<ExpectedSplitTextResult>& expected_results) {
  const auto& actual_results = trigger_results.at(processors::SplitText::Splits);
  REQUIRE(actual_results.size() == expected_results.size());
  std::string identifier;
  for (size_t i = 0; i < expected_results.size(); ++i) {
    CHECK(controller.plan->getContent(actual_results[i]) == expected_results[i].content);

    CHECK(actual_results[i]->getAttribute(processors::SplitText::TextLineCountOutputAttribute.name) == std::to_string(expected_results[i].text_line_count));
    CHECK(actual_results[i]->getAttribute(processors::SplitText::FragmentSizeOutputAttribute.name) == std::to_string(expected_results[i].content.size()));
    if (i > 0) {
      CHECK(actual_results[i]->getAttribute(processors::SplitText::FragmentIdentifierOutputAttribute.name).value() == identifier);
    } else {
      identifier = actual_results[i]->getAttribute(processors::SplitText::FragmentIdentifierOutputAttribute.name).value();
      CHECK(!identifier.empty());
    }
    CHECK(actual_results[i]->getAttribute(core::SpecialFlowAttribute::FILENAME) ==
      "a.foo.fragment." + identifier + "." + std::to_string(expected_results[i].fragment_index));
    CHECK(actual_results[i]->getAttribute(processors::SplitText::FragmentIndexOutputAttribute.name) == std::to_string(expected_results[i].fragment_index));
    CHECK(actual_results[i]->getAttribute(processors::SplitText::FragmentCountOutputAttribute.name) == std::to_string(expected_results[i].fragment_count));
    CHECK(actual_results[i]->getAttribute(processors::SplitText::SegmentOriginalFilenameOutputAttribute.name) == "a.foo");
  }
}

void runSplitTextTest(const std::string& input, const std::vector<ExpectedSplitTextResult>& expected_results, const SplitTextProperties& properties) {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  const auto split_text = controller.getProcessor();
  REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, std::to_string(properties.line_split_count)));
  if (properties.maximum_fragment_size) {
    REQUIRE(split_text->setProperty(processors::SplitText::MaximumFragmentSize.name, std::to_string(*properties.maximum_fragment_size) + " B"));
  }
  if (properties.trim_trailing_newlines) {
    REQUIRE(split_text->setProperty(processors::SplitText::RemoveTrailingNewlines.name, properties.trim_trailing_newlines.value() ? "true" : "false"));
  }
  if (properties.header_line_count) {
    REQUIRE(split_text->setProperty(processors::SplitText::HeaderLineCount.name, std::to_string(*properties.header_line_count)));
  }
  if (properties.header_line_marker_characters) {
    REQUIRE(split_text->setProperty(processors::SplitText::HeaderLineMarkerCharacters.name, *properties.header_line_marker_characters));
  }
  const auto trigger_results = controller.trigger(input, {{std::string(core::SpecialFlowAttribute::FILENAME), "a.foo"}});
  CHECK(trigger_results.at(processors::SplitText::Failure).empty());
  CHECK(trigger_results.at(processors::SplitText::Original).size() == 1);
  CHECK(trigger_results.at(processors::SplitText::Original)[0]->getAttribute(core::SpecialFlowAttribute::FILENAME) == "a.foo");
  CHECK(controller.plan->getContent(trigger_results.at(processors::SplitText::Original)[0]) == input);
  verifySplitResults(controller, trigger_results, expected_results);
}

TEST_CASE("Line Split Count property is required") {
SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  REQUIRE_THROWS_WITH(controller.trigger("", {}), "Expected parsable uint64_t from \"Line Split Count\", but got PropertyNotSet (Property Error:2)");
}

TEST_CASE("Line Split Count property can only be 0 if Maximum Fragment Size is set") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  const auto split_text = controller.getProcessor();
  REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, "0"));
  REQUIRE_THROWS_AS(controller.trigger("", {}), minifi::Exception);
}

TEST_CASE("Maximum Fragment Size cannot be set to 0") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  const auto split_text = controller.getProcessor();
  REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, "0"));
  REQUIRE(split_text->setProperty(processors::SplitText::MaximumFragmentSize.name, "0 B"));
  REQUIRE_THROWS_AS(controller.trigger("", {}), minifi::Exception);
}

TEST_CASE("Header Line Marker Characters size cannot be equal or larger than split text buffer size") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  const auto split_text = controller.getProcessor();
  REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, "1"));
  std::string header_marker_character(BUFFER_SIZE, 'A');
  REQUIRE(split_text->setProperty(processors::SplitText::HeaderLineMarkerCharacters.name, header_marker_character));
  REQUIRE_THROWS_AS(controller.trigger("", {}), minifi::Exception);
}


TEST_CASE("SplitText only forwards empty flowfile") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  const auto split_text = controller.getProcessor();
  REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, "1"));
  const auto trigger_results = controller.trigger("", {{std::string(core::SpecialFlowAttribute::FILENAME), "a.foo"}});
  CHECK(trigger_results.at(processors::SplitText::Splits).empty());
  CHECK(trigger_results.at(processors::SplitText::Failure).empty());
  CHECK(trigger_results.at(processors::SplitText::Original).size() == 1);
  CHECK(trigger_results.at(processors::SplitText::Original)[0]->getAttribute(core::SpecialFlowAttribute::FILENAME) == "a.foo");
  CHECK(controller.plan->getContent(trigger_results.at(processors::SplitText::Original)[0]).empty());
}

TEST_CASE("SplitText creates new flow file for a single line") {
  std::vector<ExpectedSplitTextResult> expected_results(1, ExpectedSplitTextResult{});
  expected_results[0].fragment_index = 1;
  expected_results[0].fragment_count = 1;
  expected_results[0].text_line_count = 1;
  std::string line;
  SECTION("Empty line with LF endline") {
    line = "\n";
    expected_results[0].content = line;
    expected_results[0].text_line_count = 0;
  }
  SECTION("LF endline") {
    line = "this is a new line\n";
    expected_results[0].content = line;
  }
  SECTION("CRLF endline") {
    line = "this is a new line\r\n";
    expected_results[0].content = line;
  }
  SECTION("Line size larger than buffer size") {
    line = std::string(static_cast<size_t>(BUFFER_SIZE * 1.5), 'a') + "\n";
    expected_results[0].content = line;
  }
  SECTION("Content without endline is a single line") {
    line = "this is a new line";
    expected_results[0].content = line;
  }

  SplitTextProperties properties;
  properties.line_split_count = 1;
  properties.trim_trailing_newlines = false;
  runSplitTextTest(line, expected_results, properties);
}

TEST_CASE("SplitText creates new flow file with 2 lines") {
  std::vector<ExpectedSplitTextResult> expected_results(1, ExpectedSplitTextResult{});
  expected_results[0].fragment_index = 1;
  expected_results[0].fragment_count = 1;
  expected_results[0].text_line_count = 2;
  std::string input;
  bool remove_trailing_endline = false;
  SECTION("Only LF endlines") {
    input = "\n\n";
    expected_results[0].text_line_count = 0;
    expected_results[0].content = input;
  }
  SECTION("LF endline") {
    input = "this is a new line\nand another line\n";
    expected_results[0].content = input;
  }
  SECTION("LF endline removing trailing endlines") {
    input = "this is a new line\nand another line\n\n";
    remove_trailing_endline = true;
    expected_results[0].content = "this is a new line\nand another line";
  }
  SECTION("CRLF endline") {
    input = "this is a new line\r\nand another line\r\n";
    expected_results[0].content = input;
  }
  SECTION("CRLF endline removing trailing endlines") {
    input = "this is a new line\r\nand another line\r\n\r\n";
    remove_trailing_endline = true;
    expected_results[0].content = "this is a new line\r\nand another line";
  }
  SECTION("Line size larger than buffer size") {
    std::string str(static_cast<size_t>(BUFFER_SIZE * 1.5), 'a');
    input = str + "\n" + str + "\n";
    expected_results[0].content = input;
  }
  SECTION("Line size larger than buffer size without endline at the end") {
    std::string str(static_cast<size_t>(BUFFER_SIZE * 1.5), 'a');
    input = str + "\n" + str;
    expected_results[0].content = input;
  }

  SplitTextProperties properties;
  properties.line_split_count = 2;
  properties.trim_trailing_newlines = remove_trailing_endline;
  runSplitTextTest(input, expected_results, properties);
}

TEST_CASE("SplitText creates separate flow files from 2 lines") {
  std::vector<ExpectedSplitTextResult> expected_results(2, ExpectedSplitTextResult{});
  expected_results[0].fragment_index = 1;
  expected_results[0].fragment_count = 2;
  expected_results[0].text_line_count = 1;
  expected_results[1].fragment_index = 2;
  expected_results[1].fragment_count = 2;
  expected_results[1].text_line_count = 1;
  std::string input;
  SECTION("Only LF endline") {
    input = "\n\n";
    expected_results[0].content = "\n";
    expected_results[0].text_line_count = 0;
    expected_results[1].content = "\n";
    expected_results[1].text_line_count = 0;
  }
  SECTION("LF endline") {
    input = "this is a new line\nand another line\n";
    expected_results[0].content = "this is a new line\n";
    expected_results[1].content = "and another line\n";
  }
  SECTION("CRLF endline") {
    input = "this is a new line\r\nand another line\r\n";
    expected_results[0].content = "this is a new line\r\n";
    expected_results[1].content = "and another line\r\n";
  }
  SECTION("Line size larger than buffer size") {
    std::string str(static_cast<size_t>(BUFFER_SIZE * 1.5), 'a');
    input = str + "\n" + str + "\n";
    expected_results[0].content = str + "\n";
    expected_results[1].content = str + "\n";
  }
  SECTION("Line size larger than buffer size without endline at the end") {
    std::string str(static_cast<size_t>(BUFFER_SIZE * 1.5), 'a');
    input = str + "\n" + str;
    expected_results[0].content = str + "\n";
    expected_results[1].content = str;
  }

  SplitTextProperties properties;
  properties.line_split_count = 1;
  properties.trim_trailing_newlines = false;
  runSplitTextTest(input, expected_results, properties);
}

TEST_CASE("Endlines are trimmed when Remove Trailing Newlines is set to true and splitting by lines") {
  std::vector<ExpectedSplitTextResult> expected_results;
  uint64_t line_split_count = 0;
  std::string input;
  SECTION("Only newlines") {
    input = "\n\n\n\n\n\n\n";
    line_split_count = 3;
  }
  SECTION("Starting new lines are removed") {
    input = "\n\n\n\nline1\nline2\nline3\n\n\n";
    line_split_count = 3;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "\nline1\nline2",
      .fragment_index = 1,
      .fragment_count = 2,
      .text_line_count = 2
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "line3",
      .fragment_index = 2,
      .fragment_count = 2,
      .text_line_count = 1
    });
  }
  SECTION("Endline types are mixed") {
    input = "\n\r\n\n\r\nline1\nline2\r\nline3\r\n\n";
    line_split_count = 3;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "\r\nline1\nline2",
      .fragment_index = 1,
      .fragment_count = 2,
      .text_line_count = 2
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "line3",
      .fragment_index = 2,
      .fragment_count = 2,
      .text_line_count = 1
    });
  }

  SplitTextProperties properties;
  properties.line_split_count = line_split_count;
  properties.trim_trailing_newlines = true;
  runSplitTextTest(input, expected_results, properties);
}

TEST_CASE("If flowfile is empty after trailing new lines are removed then flow file is not emitted") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  const auto split_text = controller.getProcessor();
  SECTION("Line split count 1") {
    REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, "1"));
  }
  SECTION("Line split count 2") {
    REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, "2"));
  }
  const auto trigger_results = controller.trigger("\n\n", {{std::string(core::SpecialFlowAttribute::FILENAME), "a.foo"}});
  CHECK(trigger_results.at(processors::SplitText::Splits).empty());
  CHECK(trigger_results.at(processors::SplitText::Failure).empty());
  CHECK(trigger_results.at(processors::SplitText::Original).size() == 1);
  CHECK(trigger_results.at(processors::SplitText::Original)[0]->getAttribute(core::SpecialFlowAttribute::FILENAME) == "a.foo");
  CHECK(controller.plan->getContent(trigger_results.at(processors::SplitText::Original)[0]) == "\n\n");
}

TEST_CASE("Test Maximum Fragment Size without Line Split Count") {
  std::vector<ExpectedSplitTextResult> expected_results;
  std::string input;
  std::optional<uint64_t> maximum_fragment_size;
  bool trim_trailing_newlines = false;
  SECTION("When line split count is zero only max fragment size is used") {
    input = "this is a new line\nand another line\nthirdline\n";
    maximum_fragment_size = 40;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "this is a new line\nand another line\n",
      .fragment_index = 1,
      .fragment_count = 2,
      .text_line_count = 2
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "thirdline\n",
      .fragment_index = 2,
      .fragment_count = 2,
      .text_line_count = 1
    });
  }
  SECTION("Max fragment size is larger than the input") {
    input = "this is a new line\nand another line\nthirdline\n";
    maximum_fragment_size = 100;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = input,
      .fragment_index = 1,
      .fragment_count = 1,
      .text_line_count = 3
    });
  }
  SECTION("When max fragment size limit is reached with only empty lines, flowfile should not be emitted") {
    input = "\n\nthis is a new line\n\n\nand another line\n";
    maximum_fragment_size = 2;
    trim_trailing_newlines = true;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "this is a new line",
      .fragment_index = 1,
      .fragment_count = 2,
      .text_line_count = 1
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "and another line",
      .fragment_index = 2,
      .fragment_count = 2,
      .text_line_count = 1
    });
  }
  SECTION("When max fragment size limit is reached and only empty lines are present, flowfile should be emitted if trailing newlines are not removed") {
    input = "\n\nthis is a new line\n\nand another line\n";
    maximum_fragment_size = 2;
    trim_trailing_newlines = false;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "\n\n",
      .fragment_index = 1,
      .fragment_count = 4,
      .text_line_count = 0
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "this is a new line\n",
      .fragment_index = 2,
      .fragment_count = 4,
      .text_line_count = 1
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "\n",
      .fragment_index = 3,
      .fragment_count = 4,
      .text_line_count = 0
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "and another line\n",
      .fragment_index = 4,
      .fragment_count = 4,
      .text_line_count = 1
    });
  }
  SECTION("Fragment index should not be incremented for removed only-newline fragments") {
    input = "trim\n\n\n\n\n\nand another line\n\n";
    maximum_fragment_size = 5;
    trim_trailing_newlines = true;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "trim",
      .fragment_index = 1,
      .fragment_count = 2,
      .text_line_count = 1
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "and another line",
      .fragment_index = 2,
      .fragment_count = 2,
      .text_line_count = 1
    });
  }

  SplitTextProperties properties;
  properties.line_split_count = 0;
  properties.trim_trailing_newlines = trim_trailing_newlines;
  properties.maximum_fragment_size = maximum_fragment_size;
  runSplitTextTest(input, expected_results, properties);
}

TEST_CASE("Test Maximum Fragment Size together with Line Split Count") {
  std::vector<ExpectedSplitTextResult> expected_results;
  std::string input;
  uint64_t line_split_count = 0;
  uint64_t maximum_fragment_size = 0;
  SECTION("Maximum fragment size reaches the limit first") {
    input = "this is a new line\nand another line\nthirdline\n";
    line_split_count = 3;
    maximum_fragment_size = 40;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "this is a new line\nand another line\n",
      .fragment_index = 1,
      .fragment_count = 2,
      .text_line_count = 2
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "thirdline\n",
      .fragment_index = 2,
      .fragment_count = 2,
      .text_line_count = 1
    });
  }
  SECTION("Maximum fragment size reaches the limit before the first line") {
    input = "this is a new line\nand another line\nthirdline\n";
    line_split_count = 2;
    maximum_fragment_size = 13;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "this is a new line\n",
      .fragment_index = 1,
      .fragment_count = 3,
      .text_line_count = 1
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "and another line\n",
      .fragment_index = 2,
      .fragment_count = 3,
      .text_line_count = 1
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "thirdline\n",
      .fragment_index = 3,
      .fragment_count = 3,
      .text_line_count = 1
    });
  }
  SECTION("Line split count reaches the limit before the max fragment size") {
    input = "this is a new line\nand another line\nthirdline\n";
    line_split_count = 2;
    maximum_fragment_size = 50;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "this is a new line\nand another line\n",
      .fragment_index = 1,
      .fragment_count = 2,
      .text_line_count = 2
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "thirdline\n",
      .fragment_index = 2,
      .fragment_count = 2,
      .text_line_count = 1
    });
  }

  SplitTextProperties properties;
  properties.line_split_count = line_split_count;
  properties.trim_trailing_newlines = false;
  properties.maximum_fragment_size = maximum_fragment_size;
  runSplitTextTest(input, expected_results, properties);
}

TEST_CASE("If the header defined by the header line count is larger than the flow file line count then the processor should fail") {
  std::string input;
  SECTION("Empty flow file") {
    input = "";
  }
  SECTION("Header line count is one line shorter larger than the flow file") {
    input = "header line 1\nheader line 2\nthis is a new line\n";
  }
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  const auto split_text = controller.getProcessor();
  REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, "1"));
  REQUIRE(split_text->setProperty(processors::SplitText::HeaderLineCount.name, "4"));
  const auto trigger_results = controller.trigger(input, {{std::string(core::SpecialFlowAttribute::FILENAME), "a.foo"}});
  CHECK(trigger_results.at(processors::SplitText::Splits).empty());
  REQUIRE(trigger_results.at(processors::SplitText::Original).empty());
  REQUIRE(trigger_results.at(processors::SplitText::Failure).size() == 1);
  CHECK(controller.plan->getContent(trigger_results.at(processors::SplitText::Failure)[0]) == input);
}

TEST_CASE("If the header defined by the header line count is larger than the max fragment size then the processor should fail") {
  std::string input;
  input = "header line 1\nheader line 2\nthis is a new line\n";
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  const auto split_text = controller.getProcessor();
  REQUIRE(split_text->setProperty(processors::SplitText::MaximumFragmentSize.name, "20 B"));
  REQUIRE(split_text->setProperty(processors::SplitText::HeaderLineCount.name, "2"));
  REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, "0"));
  const auto trigger_results = controller.trigger(input, {{std::string(core::SpecialFlowAttribute::FILENAME), "a.foo"}});
  CHECK(trigger_results.at(processors::SplitText::Splits).empty());
  REQUIRE(trigger_results.at(processors::SplitText::Original).empty());
  REQUIRE(trigger_results.at(processors::SplitText::Failure).size() == 1);
  CHECK(controller.plan->getContent(trigger_results.at(processors::SplitText::Failure)[0]) == input);
}

TEST_CASE("If header line count is the same as the flow file line count then no new flow file should be emitted") {
  std::string input = "header line 1\nheader line 2\nthis is a new line\n";
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  const auto split_text = controller.getProcessor();
  REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, "1"));
  REQUIRE(split_text->setProperty(processors::SplitText::HeaderLineCount.name, "3"));
  const auto trigger_results = controller.trigger(input, {{std::string(core::SpecialFlowAttribute::FILENAME), "a.foo"}});
  CHECK(trigger_results.at(processors::SplitText::Splits).empty());
  REQUIRE(trigger_results.at(processors::SplitText::Failure).empty());
  REQUIRE(trigger_results.at(processors::SplitText::Original).size() == 1);
  CHECK(trigger_results.at(processors::SplitText::Original)[0]->getAttribute(core::SpecialFlowAttribute::FILENAME) == "a.foo");
  CHECK(controller.plan->getContent(trigger_results.at(processors::SplitText::Original)[0]) == input);
}

TEST_CASE("Append generated flow files with specified number of header lines") {
  std::vector<ExpectedSplitTextResult> expected_results;
  std::string input = "header line 1\nheader line 2\nthis is a new line\nand another line\n";
  uint64_t line_split_count = 0;
  bool trim_trailing_newlines = false;
  SECTION("Line split count 1") {
    line_split_count = 1;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "header line 1\nheader line 2\nthis is a new line\n",
      .fragment_index = 1,
      .fragment_count = 2,
      .text_line_count = 1
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "header line 1\nheader line 2\nand another line\n",
      .fragment_index = 2,
      .fragment_count = 2,
      .text_line_count = 1
    });
  }
  SECTION("Line split count 2") {
    line_split_count = 2;
    trim_trailing_newlines = true;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "header line 1\nheader line 2\nthis is a new line\nand another line",
      .fragment_index = 1,
      .fragment_count = 1,
      .text_line_count = 2
    });
  }
  runSplitTextTest(input, expected_results, SplitTextProperties{
    .line_split_count = line_split_count,
    .trim_trailing_newlines = trim_trailing_newlines,
    .maximum_fragment_size = std::nullopt,
    .header_line_count = 2,
    .header_line_marker_characters = "ignored"
  });
}

TEST_CASE("If a split fragment would only consist of new lines then only the trimmed header should be emitted") {
  std::vector<ExpectedSplitTextResult> expected_results;
  std::string input = "header line 1\n\nline1\nline2\n\n\nline3\nline4\n\n\n\n";
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "header line 1\n\nline1\nline2",
    .fragment_index = 1,
    .fragment_count = 5,
    .text_line_count = 2
  });
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "header line 1",
    .fragment_index = 2,
    .fragment_count = 5,
    .text_line_count = 0
  });
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "header line 1\n\nline3\nline4",
    .fragment_index = 3,
    .fragment_count = 5,
    .text_line_count = 2
  });
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "header line 1",
    .fragment_index = 4,
    .fragment_count = 5,
    .text_line_count = 0
  });
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "header line 1",
    .fragment_index = 5,
    .fragment_count = 5,
    .text_line_count = 0
  });

  runSplitTextTest(input, expected_results, SplitTextProperties{
    .line_split_count = 2,
    .trim_trailing_newlines = true,
    .maximum_fragment_size = std::nullopt,
    .header_line_count = 2,
    .header_line_marker_characters = "ignored"
  });
}

TEST_CASE("Append generated flow files with header lines specified with header line marker characters") {
  std::vector<ExpectedSplitTextResult> expected_results;
  std::string input = "header line 1\nheader line 2\nthis is a new line\nand another line\n";
  uint64_t line_split_count = 0;
  bool trim_trailing_newlines = false;
  SECTION("Line split count 1") {
    line_split_count = 1;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "header line 1\nheader line 2\nthis is a new line\n",
      .fragment_index = 1,
      .fragment_count = 2,
      .text_line_count = 1
    });
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "header line 1\nheader line 2\nand another line\n",
      .fragment_index = 2,
      .fragment_count = 2,
      .text_line_count = 1
    });
  }
  SECTION("Line split count 2") {
    line_split_count = 2;
    trim_trailing_newlines = true;
    expected_results.push_back(ExpectedSplitTextResult{
      .content = "header line 1\nheader line 2\nthis is a new line\nand another line",
      .fragment_index = 1,
      .fragment_count = 1,
      .text_line_count = 2
    });
  }
  runSplitTextTest(input, expected_results, SplitTextProperties{
    .line_split_count = line_split_count,
    .trim_trailing_newlines = trim_trailing_newlines,
    .maximum_fragment_size = std::nullopt,
    .header_line_count = std::nullopt,
    .header_line_marker_characters = "hea"
  });
}

TEST_CASE("If a split fragment would only consist of new lines then only the trimmed header should be emitted when using header line marker characters") {
  std::vector<ExpectedSplitTextResult> expected_results;
  std::string input = "header line 1\nvery long line should be splitted before split line count\nline2\n\n\n\nline3\nline4\n\n";
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "header line 1\nvery long line should be splitted before split line count",
    .fragment_index = 1,
    .fragment_count = 5,
    .text_line_count = 1
  });
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "header line 1\nline2",
    .fragment_index = 2,
    .fragment_count = 5,
    .text_line_count = 1
  });
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "header line 1",
    .fragment_index = 3,
    .fragment_count = 5,
    .text_line_count = 0
  });
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "header line 1\nline3\nline4",
    .fragment_index = 4,
    .fragment_count = 5,
    .text_line_count = 2
  });
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "header line 1",
    .fragment_index = 5,
    .fragment_count = 5,
    .text_line_count = 0
  });

  runSplitTextTest(input, expected_results, SplitTextProperties{
    .line_split_count = 2,
    .trim_trailing_newlines = true,
    .maximum_fragment_size = 30,
    .header_line_count = std::nullopt,
    .header_line_marker_characters = "hea"
  });
}

TEST_CASE("If the header defined by header marker characters is larger than the max fragments size then the processor should fail") {
  std::string input;
  input = "header line 1\nheader line 2\nthis is a new line\n";
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  const auto split_text = controller.getProcessor();
  REQUIRE(split_text->setProperty(processors::SplitText::MaximumFragmentSize.name, "20 B"));
  REQUIRE(split_text->setProperty(processors::SplitText::HeaderLineMarkerCharacters.name, "hea"));
  REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, "0"));
  const auto trigger_results = controller.trigger(input, {{std::string(core::SpecialFlowAttribute::FILENAME), "a.foo"}});
  CHECK(trigger_results.at(processors::SplitText::Splits).empty());
  REQUIRE(trigger_results.at(processors::SplitText::Original).empty());
  REQUIRE(trigger_results.at(processors::SplitText::Failure).size() == 1);
  CHECK(controller.plan->getContent(trigger_results.at(processors::SplitText::Failure)[0]) == input);
}

TEST_CASE("If the header defined by header marker characters is the only content in the flow file then the processor should not emit new flow files") {
  std::string input;
  input = "header line 1\nheader line 2\n";
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::SplitText>("SplitText")};
  const auto split_text = controller.getProcessor();
  REQUIRE(split_text->setProperty(processors::SplitText::MaximumFragmentSize.name, "40 B"));
  REQUIRE(split_text->setProperty(processors::SplitText::HeaderLineMarkerCharacters.name, "hea"));
  REQUIRE(split_text->setProperty(processors::SplitText::LineSplitCount.name, "0"));
  const auto trigger_results = controller.trigger(input, {{std::string(core::SpecialFlowAttribute::FILENAME), "a.foo"}});
  CHECK(trigger_results.at(processors::SplitText::Splits).empty());
  REQUIRE(trigger_results.at(processors::SplitText::Failure).empty());
  REQUIRE(trigger_results.at(processors::SplitText::Original).size() == 1);
  CHECK(controller.plan->getContent(trigger_results.at(processors::SplitText::Original)[0]) == input);
}

TEST_CASE("Header lines should be counted as part of the fragment size when maximum fragment size is specified") {
  std::vector<ExpectedSplitTextResult> expected_results;
  std::string input = "[header] hline 1\nline 2\nline 3\n";
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "[header] hline 1\nline 2",
    .fragment_index = 1,
    .fragment_count = 2,
    .text_line_count = 1
  });
  expected_results.push_back(ExpectedSplitTextResult{
    .content = "[header] hline 1\nline 3",
    .fragment_index = 2,
    .fragment_count = 2,
    .text_line_count = 1
  });

  runSplitTextTest(input, expected_results, SplitTextProperties{
    .line_split_count = 2,
    .trim_trailing_newlines = true,
    .maximum_fragment_size = 30,
    .header_line_count = std::nullopt,
    .header_line_marker_characters = "[header]"
  });
}

}  // namespace org::apache::nifi::minifi::test
