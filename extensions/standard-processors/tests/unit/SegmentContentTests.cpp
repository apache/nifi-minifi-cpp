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

#include <random>

#include "minifi-cpp/FlowFileRecord.h"
#include "catch2/generators/catch_generators.hpp"
#include "processors/SegmentContent.h"
#include "range/v3/algorithm/equal.hpp"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "range/v3/algorithm/generate.hpp"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::processors::test {

std::vector<std::byte> generateRandomData(const size_t n) {
  std::independent_bits_engine<std::default_random_engine, CHAR_BIT, uint16_t> rbe{gsl::narrow_cast<uint16_t>(std::chrono::system_clock::now().time_since_epoch().count())};
  std::vector<std::byte> bytes(n);
  ranges::generate(bytes, [&]() { return static_cast<std::byte>(rbe()); });
  return bytes;
}

std::string_view calcExpectedSegment(const std::string_view original_content, const size_t segment_i, const size_t segment_size) {
  const auto start_pos = segment_i * segment_size;
  const auto end_pos = std::min(start_pos + segment_size, original_content.length());
  const auto actual_size = std::min(segment_size, end_pos - start_pos);
  return original_content.substr(segment_i * segment_size, std::min(segment_size, actual_size));
}

std::span<const std::byte> calcExpectedSegment(const std::span<const std::byte> original_content, const size_t segment_i, const size_t segment_size) {
  const auto start_pos = segment_i * segment_size;
  const auto end_pos = std::min(start_pos + segment_size, original_content.size());
  const auto actual_size = std::min(segment_size, end_pos - start_pos);
  return original_content.subspan(segment_i * segment_size, std::min(segment_size, actual_size));
}

template<typename... Bytes>
std::vector<std::byte> createByteVector(Bytes... bytes) {
  return {static_cast<std::byte>(bytes)...};
}

TEST_CASE("Invalid segmentSize tests") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SegmentContent>("SegmentContent")};
  const auto segment_content = controller.getProcessor();

  SECTION("foo") {
    REQUIRE(segment_content->setProperty(SegmentContent::SegmentSize.name, "foo"));
    REQUIRE_THROWS_WITH(controller.trigger("bar"), "Expected parsable data size from \"Segment Size\", but got GeneralParsingError (Parsing Error:0)");
  }
  SECTION("10 foo") {
    REQUIRE(segment_content->setProperty(SegmentContent::SegmentSize.name, "10 foo"));
    REQUIRE_THROWS_WITH(controller.trigger("bar"), "Expected parsable data size from \"Segment Size\", but got GeneralParsingError (Parsing Error:0)");
  }
  SECTION("0") {
    REQUIRE(segment_content->setProperty(SegmentContent::SegmentSize.name, "0"));
    REQUIRE_THROWS_WITH(controller.trigger("bar"), "Processor Operation: Invalid Segment Size: '0'");
  }
  SECTION("10 MB") {
    REQUIRE(segment_content->setProperty(SegmentContent::SegmentSize.name, "10 MB"));
    REQUIRE_NOTHROW(controller.trigger("bar"));
  }
}

TEST_CASE("EmptyFlowFile") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SegmentContent>("SegmentContent")};
  const auto segment_content = controller.getProcessor();

  REQUIRE(segment_content->setProperty(SegmentContent::SegmentSize.name, "10 B"));

  auto trigger_results = controller.trigger("");
  auto original = trigger_results.at(processors::SegmentContent::Original);
  auto splits = trigger_results.at(processors::SegmentContent::Segments);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.empty());

  CHECK(controller.plan->getContent(original[0]).empty());
}

TEST_CASE("SegmentContent with different sized text input") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SegmentContent>("SegmentContent")};
  const auto segment_content = controller.getProcessor();

  auto [original_size, segment_size] = GENERATE(
    std::make_tuple(size_t{1020}, size_t{30}),
    std::make_tuple(1020, 31),
    std::make_tuple(1020, 1),
    std::make_tuple(2000, 30),
    std::make_tuple(2000, 1010),
    std::make_tuple(2000, 1050),
    std::make_tuple(100, 100),
    std::make_tuple(99, 100),
    std::make_tuple(100, 99));

  const std::string original_content = utils::string::repeat("a", original_size);

  REQUIRE(segment_content->setProperty(SegmentContent::SegmentSize.name, std::to_string(segment_size)));

  auto trigger_results = controller.trigger(original_content);

  auto original = trigger_results.at(processors::SegmentContent::Original);
  auto segments = trigger_results.at(processors::SegmentContent::Segments);

  auto expected_segment_size = gsl::narrow<size_t>(std::ceil(static_cast<double>(original_size) / static_cast<double>(segment_size)));
  REQUIRE(segments.size() == expected_segment_size);
  REQUIRE(original.size() == 1);

  size_t segment_size_sum = 0;
  for (size_t segment_i = 0; segment_i < expected_segment_size; ++segment_i) {
    auto segment_str = controller.plan->getContent(segments[segment_i]);
    CHECK(segment_str == calcExpectedSegment(original_content, segment_i, segment_size));
    segment_size_sum += segment_str.length();
  }
  CHECK(original_size == segment_size_sum);
}

TEST_CASE("SegmentContent with different sized byte input") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SegmentContent>("SegmentContent")};
  const auto segment_content = controller.getProcessor();

  auto [original_size, segment_size] = GENERATE(
    std::make_tuple(size_t{1020}, size_t{30}),
    std::make_tuple(1020, 31),
    std::make_tuple(1020, 1),
    std::make_tuple(2000, 30),
    std::make_tuple(2000, 1010),
    std::make_tuple(2000, 1050),
    std::make_tuple(100, 100),
    std::make_tuple(99, 100),
    std::make_tuple(100, 99));

  const auto input_data = generateRandomData(original_size);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  REQUIRE(segment_content->setProperty(SegmentContent::SegmentSize.name, std::to_string(segment_size)));

  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SegmentContent::Original);
  auto segments = trigger_results.at(processors::SegmentContent::Segments);

  auto expected_segment_size = gsl::narrow<size_t>(std::ceil(static_cast<double>(original_size) / static_cast<double>(segment_size)));
  REQUIRE(segments.size() == expected_segment_size);
  REQUIRE(original.size() == 1);

  size_t segment_size_sum = 0;
  for (size_t segment_i = 0; segment_i < expected_segment_size; ++segment_i) {
    auto segment_bytes = controller.plan->getContentAsBytes(*segments[segment_i]);
    CHECK(ranges::equal(segment_bytes, calcExpectedSegment(input_data, segment_i, segment_size)));
    segment_size_sum += segment_bytes.size();
  }
  CHECK(original_size == segment_size_sum);
}

TEST_CASE("SimpleTest", "[NiFi]") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SegmentContent>("SegmentContent")};
  const auto segment_content = controller.getProcessor();

  REQUIRE(segment_content->setProperty(SegmentContent::SegmentSize.name, "4 B"));

  const auto input_data = createByteVector(1, 2, 3, 4, 5, 6, 7, 8, 9);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input, {{std::string{core::SpecialFlowAttribute::UUID}, "original_uuid"}});

  auto original = trigger_results.at(processors::SegmentContent::Original);
  auto segments = trigger_results.at(processors::SegmentContent::Segments);

  REQUIRE(segments.size() == 3);
  REQUIRE(original.size() == 1);

  auto expected_segment_1 = createByteVector(1, 2, 3, 4);
  auto expected_segment_2 = createByteVector(5, 6, 7, 8);
  auto expected_segment_3 = createByteVector(9);

  CHECK(controller.plan->getContentAsBytes(*original[0]) == input_data);
  CHECK(controller.plan->getContentAsBytes(*segments[0]) == expected_segment_1);
  CHECK(controller.plan->getContentAsBytes(*segments[1]) == expected_segment_2);
  CHECK(controller.plan->getContentAsBytes(*segments[2]) == expected_segment_3);

  auto flowfile_filename = *original[0]->getAttribute(core::SpecialFlowAttribute::FILENAME);

  CHECK(segments[0]->getAttribute(SegmentContent::SegmentOriginalFilenameOutputAttribute.name) == flowfile_filename);
  CHECK(segments[1]->getAttribute(SegmentContent::SegmentOriginalFilenameOutputAttribute.name) == flowfile_filename);
  CHECK(segments[2]->getAttribute(SegmentContent::SegmentOriginalFilenameOutputAttribute.name) == flowfile_filename);

  CHECK(segments[0]->getAttribute(SegmentContent::FragmentIdentifierOutputAttribute.name) == "original_uuid");
  CHECK(segments[1]->getAttribute(SegmentContent::FragmentIdentifierOutputAttribute.name) == "original_uuid");
  CHECK(segments[2]->getAttribute(SegmentContent::FragmentIdentifierOutputAttribute.name) == "original_uuid");

  CHECK(segments[0]->getAttribute(SegmentContent::FragmentCountOutputAttribute.name) == "3");
  CHECK(segments[1]->getAttribute(SegmentContent::FragmentCountOutputAttribute.name) == "3");
  CHECK(segments[2]->getAttribute(SegmentContent::FragmentCountOutputAttribute.name) == "3");

  CHECK(segments[0]->getAttribute(SegmentContent::FragmentIndexOutputAttribute.name) == "1");
  CHECK(segments[1]->getAttribute(SegmentContent::FragmentIndexOutputAttribute.name) == "2");
  CHECK(segments[2]->getAttribute(SegmentContent::FragmentIndexOutputAttribute.name) == "3");
}

TEST_CASE("TransferSmall", "[NiFi]") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SegmentContent>("SegmentContent")};
  const auto segment_content = controller.getProcessor();

  REQUIRE(segment_content->setProperty(SegmentContent::SegmentSize.name, "4 KB"));

  const auto input_data = createByteVector(1, 2, 3, 4, 5, 6, 7, 8, 9);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SegmentContent::Original);
  auto segments = trigger_results.at(processors::SegmentContent::Segments);

  REQUIRE(segments.size() == 1);
  REQUIRE(original.size() == 1);

  CHECK(controller.plan->getContentAsBytes(*segments[0]) == input_data);
  CHECK(controller.plan->getContentAsBytes(*original[0]) == input_data);
}

TEST_CASE("ExpressionLanguageSupport", "[NiFi]") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SegmentContent>("SegmentContent")};
  const auto segment_content = controller.getProcessor();

  REQUIRE(segment_content->setProperty(SegmentContent::SegmentSize.name, "${segmentSize}"));

  const auto input_data = createByteVector(1, 2, 3, 4, 5, 6, 7, 8, 9);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input, {{"segmentSize", "4 B"}});

  auto original = trigger_results.at(processors::SegmentContent::Original);
  auto segments = trigger_results.at(processors::SegmentContent::Segments);

  REQUIRE(segments.size() == 3);
  REQUIRE(original.size() == 1);

  auto expected_segment_1 = createByteVector(1, 2, 3, 4);
  auto expected_segment_2 = createByteVector(5, 6, 7, 8);
  auto expected_segment_3 = createByteVector(9);

  CHECK(controller.plan->getContentAsBytes(*original[0]) == input_data);
  CHECK(controller.plan->getContentAsBytes(*segments[0]) == expected_segment_1);
  CHECK(controller.plan->getContentAsBytes(*segments[1]) == expected_segment_2);
  CHECK(controller.plan->getContentAsBytes(*segments[2]) == expected_segment_3);
}

}  // namespace org::apache::nifi::minifi::processors::test
