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

#include "FlowFileRecord.h"
#include "catch2/generators/catch_generators.hpp"
#include "processors/SplitContent.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"

namespace org::apache::nifi::minifi::processors::test {

template<typename... Bytes>
std::vector<std::byte> createByteVector(Bytes... bytes) {
  return {static_cast<std::byte>(bytes)...};
}

TEST_CASE("WithoutByteSequence") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};
  split_content->setProperty(SplitContent::ByteSequenceFormatProperty, magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text));
  split_content->setProperty(SplitContent::KeepByteSequence, "true");
  split_content->setProperty(SplitContent::ByteSequenceLocationProperty, magic_enum::enum_name(SplitContent::ByteSequenceLocation::Leading));

  REQUIRE_THROWS_WITH(controller.trigger("rub-a-dub-dub"), "General Operation: Required property is empty: Byte Sequence");
}

TEST_CASE("TextFormatLeadingPosition", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};
  split_content->setProperty(SplitContent::ByteSequenceFormatProperty, magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text));
  split_content->setProperty(SplitContent::ByteSequence, "ub");
  split_content->setProperty(SplitContent::KeepByteSequence, "true");
  split_content->setProperty(SplitContent::ByteSequenceLocationProperty, magic_enum::enum_name(SplitContent::ByteSequenceLocation::Leading));

  auto trigger_results = controller.trigger("rub-a-dub-dub");
  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 4);

  CHECK(controller.plan->getContent(original[0]) == "rub-a-dub-dub");

  CHECK(controller.plan->getContent(splits[0]) == "r");
  CHECK(controller.plan->getContent(splits[1]) == "ub-a-d");
  CHECK(controller.plan->getContent(splits[2]) == "ub-d");
  CHECK(controller.plan->getContent(splits[3]) == "ub");
}

TEST_CASE("TextFormatTrailingPosition", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::ByteSequenceFormatProperty, magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text));
  split_content->setProperty(SplitContent::ByteSequence, "ub");
  split_content->setProperty(SplitContent::KeepByteSequence, "true");
  split_content->setProperty(SplitContent::ByteSequenceLocationProperty, magic_enum::enum_name(SplitContent::ByteSequenceLocation::Trailing));

  auto trigger_results = controller.trigger("rub-a-dub-dub");

  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 3);

  CHECK(controller.plan->getContent(original[0]) == "rub-a-dub-dub");

  CHECK(controller.plan->getContent(splits[0]) == "rub");
  CHECK(controller.plan->getContent(splits[1]) == "-a-dub");
  CHECK(controller.plan->getContent(splits[2]) == "-dub");
}

TEST_CASE("TextFormatSplits", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::ByteSequenceFormatProperty, magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text));
  split_content->setProperty(SplitContent::ByteSequence, "test");

  constexpr std::string_view input_1 = "This is a test. This is another test. And this is yet another test. Finally this is the last Test.";
  constexpr std::string_view input_2 = "This is a test. This is another test. And this is yet another test. Finally this is the last test";

  const auto [keep_byte_sequence, byte_sequence_location, input, expected_splits] = GENERATE_REF(
      std::make_tuple("true", "Leading", input_1, std::vector<std::string_view>{"This is a ", "test. This is another ", "test. And this is yet another ", "test. Finally this is the last Test."}),
      std::make_tuple("false", "Leading", input_1, std::vector<std::string_view>{"This is a ", ". This is another ", ". And this is yet another ", ". Finally this is the last Test."}),
      std::make_tuple("true", "Trailing", input_1, std::vector<std::string_view>{"This is a test", ". This is another test", ". And this is yet another test", ". Finally this is the last Test."}),
      std::make_tuple("false", "Trailing", input_1, std::vector<std::string_view>{"This is a ", ". This is another ", ". And this is yet another ", ". Finally this is the last Test."}),
      std::make_tuple("true", "Leading", input_2, std::vector<std::string_view>{"This is a ", "test. This is another ", "test. And this is yet another ", "test. Finally this is the last ", "test"}),
      std::make_tuple("true", "Trailing", input_2, std::vector<std::string_view>{"This is a test", ". This is another test", ". And this is yet another test", ". Finally this is the last test"}));

  split_content->setProperty(SplitContent::KeepByteSequence, keep_byte_sequence);
  split_content->setProperty(SplitContent::ByteSequenceLocationProperty, byte_sequence_location);

  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == expected_splits.size());

  CHECK(controller.plan->getContent(original[0]) == input);

  for (size_t i = 0; i < expected_splits.size(); ++i) {
    auto split_i = controller.plan->getContent(splits[i]);
    auto expected_i = expected_splits[i];
    CHECK(split_i == expected_i);
  }
}

TEST_CASE("SmallSplits", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::KeepByteSequence, "false");
  split_content->setProperty(SplitContent::ByteSequence, "FFFF");

  const auto input_data = createByteVector(1, 2, 3, 4, 5, 0xFF, 0xFF, 0xFF, 5, 4, 3, 2, 1);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 2);

  const auto expected_split_1 = createByteVector(1, 2, 3, 4, 5);
  const auto expected_split_2 = createByteVector(0xFF, 5, 4, 3, 2, 1);

  CHECK(controller.plan->getContentAsBytes(*splits[0]) == expected_split_1);
  CHECK(controller.plan->getContentAsBytes(*splits[1]) == expected_split_2);
}

TEST_CASE("WithSingleByteSplit", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::KeepByteSequence, "false");
  split_content->setProperty(SplitContent::ByteSequence, "FF");

  const auto input_data = createByteVector(1, 2, 3, 4, 5, 0xFF, 5, 4, 3, 2, 1);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input);

  const auto original = trigger_results.at(processors::SplitContent::Original);
  const auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 2);

  const auto expected_split_1 = createByteVector(1, 2, 3, 4, 5);
  const auto expected_split_2 = createByteVector(5, 4, 3, 2, 1);

  CHECK(controller.plan->getContentAsBytes(*splits[0]) == expected_split_1);
  CHECK(controller.plan->getContentAsBytes(*splits[1]) == expected_split_2);
}

TEST_CASE("WithLargerSplit", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::KeepByteSequence, "false");
  split_content->setProperty(SplitContent::ByteSequence, "05050505");

  const auto input_data = createByteVector(1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 4, 3, 2, 1);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input);

  const auto original = trigger_results.at(processors::SplitContent::Original);
  const auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 2);

  const auto expected_split_1 = createByteVector(1, 2, 3, 4);
  const auto expected_split_2 = createByteVector(5, 5, 4, 3, 2, 1);

  CHECK(controller.plan->getContentAsBytes(*splits[0]) == expected_split_1);
  CHECK(controller.plan->getContentAsBytes(*splits[1]) == expected_split_2);
}

TEST_CASE("KeepingSequence", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::KeepByteSequence, "true");
  split_content->setProperty(SplitContent::ByteSequence, "05050505");

  const auto input_data = createByteVector(1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 4, 3, 2, 1);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input);

  const auto original = trigger_results.at(processors::SplitContent::Original);
  const auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 2);

  const auto expected_split_1 = createByteVector(1, 2, 3, 4, 5, 5, 5, 5);
  const auto expected_split_2 = createByteVector(5, 5, 4, 3, 2, 1);

  CHECK(controller.plan->getContentAsBytes(*splits[0]) == expected_split_1);
  CHECK(controller.plan->getContentAsBytes(*splits[1]) == expected_split_2);
}

TEST_CASE("EndsWithSequence", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::KeepByteSequence, "false");
  split_content->setProperty(SplitContent::ByteSequence, "05050505");

  const auto input_data = createByteVector(1, 2, 3, 4, 5, 5, 5, 5);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 1);

  auto expected_split = createByteVector(1, 2, 3, 4);

  CHECK(controller.plan->getContentAsBytes(*splits[0]) == expected_split);
}

TEST_CASE("EndsWithSequenceAndKeepSequence", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::KeepByteSequence, "true");
  split_content->setProperty(SplitContent::ByteSequence, "05050505");

  const auto input_data = createByteVector(1, 2, 3, 4, 5, 5, 5, 5);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 1);

  auto expected_split_1 = createByteVector(1, 2, 3, 4, 5, 5, 5, 5);

  CHECK(controller.plan->getContentAsBytes(*splits[0]) == expected_split_1);
}

TEST_CASE("StartsWithSequence", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::KeepByteSequence, "false");
  split_content->setProperty(SplitContent::ByteSequence, "05050505");

  const auto input_data = createByteVector(5, 5, 5, 5, 1, 2, 3, 4);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 1);

  auto expected_split = createByteVector(1, 2, 3, 4);

  CHECK(controller.plan->getContentAsBytes(*splits[0]) == expected_split);
}

TEST_CASE("StartsWithSequenceAndKeepTrailingSequence", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::KeepByteSequence, "true");
  split_content->setProperty(SplitContent::ByteSequence, "05050505");
  split_content->setProperty(SplitContent::ByteSequenceLocationProperty, "Trailing");

  const auto input_data = createByteVector(5, 5, 5, 5, 1, 2, 3, 4);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 2);

  auto expected_split_1 = createByteVector(5, 5, 5, 5);
  auto expected_split_2 = createByteVector(1, 2, 3, 4);

  CHECK(controller.plan->getContentAsBytes(*splits[0]) == expected_split_1);
  CHECK(controller.plan->getContentAsBytes(*splits[1]) == expected_split_2);
}

TEST_CASE("StartsWithSequenceAndKeepLeadingSequence") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::KeepByteSequence, "true");
  split_content->setProperty(SplitContent::ByteSequence, "05050505");
  split_content->setProperty(SplitContent::ByteSequenceLocationProperty, "Leading");

  const auto input_data = createByteVector(5, 5, 5, 5, 1, 2, 3, 4);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 1);

  CHECK(controller.plan->getContentAsBytes(*splits[0]) == input_data);
}

TEST_CASE("StartsWithDoubleSequenceAndKeepLeadingSequence") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::KeepByteSequence, "true");
  split_content->setProperty(SplitContent::ByteSequence, "05050505");
  split_content->setProperty(SplitContent::ByteSequenceLocationProperty, "Leading");

  const auto input_data = createByteVector(5, 5, 5, 5, 5, 5, 5, 5, 1, 2, 3, 4);
  std::string_view input(reinterpret_cast<const char*>(input_data.data()), input_data.size());

  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  auto expected_split_1 = createByteVector(5, 5, 5, 5);
  auto expected_split_2 = createByteVector(5, 5, 5, 5, 1, 2, 3, 4);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 2);

  CHECK(controller.plan->getContentAsBytes(*splits[0]) == expected_split_1);
  CHECK(controller.plan->getContentAsBytes(*splits[1]) == expected_split_2);
}

TEST_CASE("NoSplitterInString", "[NiFi]") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  split_content->setProperty(SplitContent::ByteSequenceFormatProperty, magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text));
  split_content->setProperty(SplitContent::ByteSequence, ",");
  split_content->setProperty(SplitContent::KeepByteSequence, "false");
  split_content->setProperty(SplitContent::ByteSequenceLocationProperty, magic_enum::enum_name(SplitContent::ByteSequenceLocation::Trailing));

  constexpr std::string_view input = "UVAT";
  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(splits.size() == 1);
  REQUIRE(original.size() == 1);

  CHECK(splits[0]->getAttribute("fragment.identifier").has_value());
  CHECK(splits[0]->getAttribute("segment.original.filename").has_value());

  CHECK(splits[0]->getAttribute("fragment.count").value() == "1");
  CHECK(splits[0]->getAttribute("fragment.index").value() == "1");

  CHECK(controller.plan->getContent(splits[0]) == input);
  CHECK(controller.plan->getContent(original[0]) == input);
}

TEST_CASE("ByteSequenceAtBufferTargetSize") {
  const auto split_content = std::make_shared<SplitContent>("SplitContent");
  minifi::test::SingleProcessorTestController controller{split_content};

  auto [pre_fix_size, separator_size, post_fix_size] = GENERATE(
    std::make_tuple(1020, 1020, 1020),
    std::make_tuple(10, 10, 1020),
    std::make_tuple(10, 1020, 10),
    std::make_tuple(10, 10, 1020),
    std::make_tuple(10, 1020, 1020),
    std::make_tuple(1020, 10, 1020),
    std::make_tuple(1020, 1020, 10),
    std::make_tuple(2000, 1020, 10));


  const std::string pre_fix = utils::string::repeat("a", pre_fix_size);
  const std::string separator = utils::string::repeat("b", separator_size);
  const std::string post_fix = utils::string::repeat("c", post_fix_size);

  split_content->setProperty(SplitContent::ByteSequenceFormatProperty, magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text));
  split_content->setProperty(SplitContent::ByteSequence, separator);
  split_content->setProperty(SplitContent::KeepByteSequence, "true");
  split_content->setProperty(SplitContent::ByteSequenceLocationProperty, "Trailing");

  auto input = pre_fix + separator + post_fix;
  auto trigger_results = controller.trigger(input);

  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(splits.size() == 2);
  REQUIRE(original.size() == 1);

  CHECK(controller.plan->getContent(splits[0]) == std::string(pre_fix) + std::string(separator));
  CHECK(controller.plan->getContent(splits[1]) == post_fix);

  CHECK(controller.plan->getContent(original[0]) == input);
}

}  // namespace org::apache::nifi::minifi::processors::test
