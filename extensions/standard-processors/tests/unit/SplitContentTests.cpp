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
#include "catch2/generators/catch_generators.hpp"
#include "processors/SplitContent.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "utils/ConfigurationUtils.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::processors::test {

template<typename... Bytes>
std::vector<std::byte> createByteVector(Bytes... bytes) {
  return {static_cast<std::byte>(bytes)...};
}

TEST_CASE("WithoutByteSequence") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Leading)}));

  REQUIRE_THROWS_WITH(controller.trigger("rub-a-dub-dub"), "Expected valid value from \"Byte Sequence\", but got PropertyNotSet (Property Error:2)");
}

TEST_CASE("EmptyFlowFile") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "ub"));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Leading)}));

  auto trigger_results = controller.trigger("");
  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.empty());

  CHECK(controller.plan->getContent(original[0]).empty());
}

TEST_CASE("TextFormatLeadingPosition", "[NiFi]") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "ub"));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Leading)}));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "ub"));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Trailing)}));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "test"));

  constexpr std::string_view input_1 = "This is a test. This is another test. And this is yet another test. Finally this is the last Test.";
  constexpr std::string_view input_2 = "This is a test. This is another test. And this is yet another test. Finally this is the last test";

  const auto [keep_byte_sequence, byte_sequence_location, input, expected_splits] = GENERATE_REF(
      std::make_tuple("true", "Leading", input_1, std::vector<std::string_view>{"This is a ", "test. This is another ", "test. And this is yet another ", "test. Finally this is the last Test."}),
      std::make_tuple("false", "Leading", input_1, std::vector<std::string_view>{"This is a ", ". This is another ", ". And this is yet another ", ". Finally this is the last Test."}),
      std::make_tuple("true", "Trailing", input_1, std::vector<std::string_view>{"This is a test", ". This is another test", ". And this is yet another test", ". Finally this is the last Test."}),
      std::make_tuple("false", "Trailing", input_1, std::vector<std::string_view>{"This is a ", ". This is another ", ". And this is yet another ", ". Finally this is the last Test."}),
      std::make_tuple("true", "Leading", input_2, std::vector<std::string_view>{"This is a ", "test. This is another ", "test. And this is yet another ", "test. Finally this is the last ", "test"}),
      std::make_tuple("true", "Trailing", input_2, std::vector<std::string_view>{"This is a test", ". This is another test", ". And this is yet another test", ". Finally this is the last test"}));

  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, keep_byte_sequence));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, byte_sequence_location));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "false"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "FFFF"));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "false"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "FF"));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "false"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "05050505"));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "05050505"));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "false"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "05050505"));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "05050505"));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "false"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "05050505"));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "05050505"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, "Trailing"));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "05050505"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, "Leading"));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "05050505"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, "Leading"));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, ","));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "false"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Trailing)}));

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
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();

  static_assert(utils::configuration::DEFAULT_BUFFER_SIZE >= 10);
  auto x = utils::configuration::DEFAULT_BUFFER_SIZE - 10;

  auto [pre_fix_size, separator_size, post_fix_size] = GENERATE_COPY(
    std::make_tuple(x, x, x),
    std::make_tuple(10, 10, x),
    std::make_tuple(10, x, 10),
    std::make_tuple(10, 10, x),
    std::make_tuple(10, x, x),
    std::make_tuple(x, 10, x),
    std::make_tuple(x, x, 10),
    std::make_tuple(2*x, x, 10));


  const std::string pre_fix = utils::string::repeat("a", pre_fix_size);
  const std::string separator = utils::string::repeat("b", separator_size);
  const std::string post_fix = utils::string::repeat("c", post_fix_size);

  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, separator));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, "Trailing"));

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

TEST_CASE("TrickyWithLeading", "[NiFi]") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "aab"));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Leading)}));

  auto trigger_results = controller.trigger("aaabc");
  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 2);

  CHECK(controller.plan->getContent(original[0]) == "aaabc");

  CHECK(controller.plan->getContent(splits[0]) == "a");
  CHECK(controller.plan->getContent(splits[1]) == "aabc");
}

TEST_CASE("TrickyWithTrailing", "[NiFi]") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "aab"));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Trailing)}));

  auto trigger_results = controller.trigger("aaabc");
  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 2);

  CHECK(controller.plan->getContent(original[0]) == "aaabc");

  CHECK(controller.plan->getContent(splits[0]) == "aaab");
  CHECK(controller.plan->getContent(splits[1]) == "c");
}

TEST_CASE("TrickierWithLeading", "[NiFi]") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "abcd"));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Leading)}));

  auto trigger_results = controller.trigger("abcabcabcdabc");
  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 2);

  CHECK(controller.plan->getContent(original[0]) == "abcabcabcdabc");

  CHECK(controller.plan->getContent(splits[0]) == "abcabc");
  CHECK(controller.plan->getContent(splits[1]) == "abcdabc");
}

TEST_CASE("TrickierWithTrailing", "[NiFi]") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "abcd"));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Trailing)}));

  auto trigger_results = controller.trigger("abcabcabcdabc");
  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 2);

  CHECK(controller.plan->getContent(original[0]) == "abcabcabcdabc");

  CHECK(controller.plan->getContent(splits[0]) == "abcabcabcd");
  CHECK(controller.plan->getContent(splits[1]) == "abc");
}

TEST_CASE("OnlyByteSequencesNoKeep", "[NiFi]") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "ab"));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "false"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Trailing)}));

  auto trigger_results = controller.trigger("ababab");
  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.empty());
}

TEST_CASE("OnlyByteSequencesTrailing", "[NiFi]") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "ab"));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Trailing)}));

  auto trigger_results = controller.trigger("ababab");
  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 3);

  CHECK(controller.plan->getContent(splits[0]) == "ab");
  CHECK(controller.plan->getContent(splits[1]) == "ab");
  CHECK(controller.plan->getContent(splits[2]) == "ab");
}

TEST_CASE("OnlyByteSequencesLeading", "[NiFi]") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<SplitContent>("SplitContent")};
  const auto split_content = controller.getProcessor();
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceFormatProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceFormat::Text)}));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequence.name, "ab"));
  REQUIRE(split_content->setProperty(SplitContent::KeepByteSequence.name, "true"));
  REQUIRE(split_content->setProperty(SplitContent::ByteSequenceLocationProperty.name, std::string{magic_enum::enum_name(SplitContent::ByteSequenceLocation::Leading)}));

  auto trigger_results = controller.trigger("ababab");
  auto original = trigger_results.at(processors::SplitContent::Original);
  auto splits = trigger_results.at(processors::SplitContent::Splits);

  REQUIRE(original.size() == 1);
  REQUIRE(splits.size() == 3);

  CHECK(controller.plan->getContent(splits[0]) == "ab");
  CHECK(controller.plan->getContent(splits[1]) == "ab");
  CHECK(controller.plan->getContent(splits[2]) == "ab");
}
}  // namespace org::apache::nifi::minifi::processors::test
