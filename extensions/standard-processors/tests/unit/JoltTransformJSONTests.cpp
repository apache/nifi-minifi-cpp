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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"
#include "processors/JoltTransformJSON.h"
#include "unit/SingleProcessorTestController.h"
#include "rapidjson/error/en.h"

namespace org::apache::nifi::minifi::test {

// NOLINTBEGIN(readability-container-size-empty)

TEST_CASE("Shiftr successful case") {
  SingleProcessorTestController controller{std::make_unique<minifi::processors::JoltTransformJSON>("JoltProc")};
  auto proc = controller.getProcessor();
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltTransform, magic_enum::enum_name(processors::jolt_transform_json::JoltTransform::Shift));
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltSpecification, R"json(
    {
      "a": "a_out",
      "b": {
        "@": "b.self",
        "$": "b.key",
        "c": "automatic array",
        "d": "automatic array",
        "&": "b_b",
        "f": {
          "&(1,0)": "b_f_b"
        }
      }
    }
  )json");

  auto res = controller.trigger(R"(
    {
      "a": 1,
      "b": {"c": 2, "d": "test", "b": [3, 4], "f": {"b": 5}}
    }
  )");

  CHECK(res[processors::JoltTransformJSON::Failure].size() == 0);
  CHECK(res[processors::JoltTransformJSON::Success].size() == 1);

  auto content = controller.plan->getContent(res.at(processors::JoltTransformJSON::Success).at(0));

  INFO(content);

  utils::verifyJSON(content, R"json(
    {
      "a_out": 1,
      "b": {
        "self": {"c": 2, "d": "test", "b": [3, 4], "f": {"b": 5}},
        "key": "b"
      },
      "automatic array": [2, "test"],
      "b_b": [3, 4],
      "b_f_b": 5
    }
  )json", true);
}

TEST_CASE("Shiftr multiple destination") {
  SingleProcessorTestController controller{std::make_unique<minifi::processors::JoltTransformJSON>("JoltProc")};
  auto proc = controller.getProcessor();
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltTransform, magic_enum::enum_name(processors::jolt_transform_json::JoltTransform::Shift));
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltSpecification, R"json(
    {
      "a": ["out1", "out2.inner"]
    }
  )json");

  auto res = controller.trigger(R"(
    {
      "a": 1
    }
  )");

  CHECK(res[processors::JoltTransformJSON::Failure].size() == 0);
  CHECK(res[processors::JoltTransformJSON::Success].size() == 1);

  utils::verifyJSON(controller.plan->getContent(res.at(processors::JoltTransformJSON::Success).at(0)), R"json(
    {
      "out1": 1,
      "out2": {"inner": 1}
    }
  )json", true);
}

TEST_CASE("Shiftr destination is a string or array of strings") {
  /// sanity check
  REQUIRE(minifi::utils::jolt::Spec::parse(R"json({"a": ["out", "out2"], "b": "out3"})json"));
  REQUIRE_FALSE(minifi::utils::jolt::Spec::parse(R"json({"a": 3})json"));
  REQUIRE_FALSE(minifi::utils::jolt::Spec::parse(R"json({"a": ["out", 1]})json"));
  REQUIRE_FALSE(minifi::utils::jolt::Spec::parse(R"json({"a": ["out", {"@": "invalid"}]})json"));
}

TEST_CASE("Shiftr template is correctly parsed") {
  using Template = minifi::utils::jolt::Spec::Template;
  {
    const std::string_view test_str = "a&0b";
    REQUIRE(Template::parse(test_str.begin(), test_str.end()).value().first == Template({"a", "b"}, {{0, 0}}));
  }
  {
    const std::string_view test_str = "a&12&(4,5)b&c";
    REQUIRE(Template::parse(test_str.begin(), test_str.end()).value().first == Template({"a", "", "b", "c"}, {{12, 0}, {4, 5}, {0, 0}}));
  }
}

TEST_CASE("Shiftr invalid reference") {
  /// sanity check
  REQUIRE(minifi::utils::jolt::Spec::parse(R"json({"a*": {"b*_*c": {"&(0,0)&(0,1)&(0,2)&(1)&(1,1)": "&(0,0)"}}, "b": "out3"})json").has_value());
}

TEST_CASE("Shiftr matches are correctly ordered") {
  SingleProcessorTestController controller{std::make_unique<minifi::processors::JoltTransformJSON>("JoltProc")};
  auto proc = controller.getProcessor();
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltTransform, magic_enum::enum_name(processors::jolt_transform_json::JoltTransform::Shift));
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltSpecification, R"json(
    {
      "a": {
        "a": {
          "c": "literal",
          "&(1,0)": "second<none>",
          "&0": "first",
          "*b*": "third",
          "*a*": "fourth"
        }
      }
    }
  )json");

  auto res = controller.trigger(R"(
    {
      "a": {
        "a": {
          "c": "c",
          "a": "a",
          "ab": "ab"
        }
      }
    }
  )");

  CHECK(res[processors::JoltTransformJSON::Failure].size() == 0);
  CHECK(res[processors::JoltTransformJSON::Success].size() == 1);

  auto content = controller.plan->getContent(res.at(processors::JoltTransformJSON::Success).at(0));

  utils::verifyJSON(content, R"json(
    {
      "literal": "c",
      "first": "a",
      "fourth": "ab"
    }
  )json", true);
}

TEST_CASE("Shiftr arrays are maps with numeric keys") {
  SingleProcessorTestController controller{std::make_unique<minifi::processors::JoltTransformJSON>("JoltProc")};
  auto proc = controller.getProcessor();
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltTransform, magic_enum::enum_name(processors::jolt_transform_json::JoltTransform::Shift));
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltSpecification, R"json(
    {
      "a": {
        "0": "a_&",
        "1": "a_&"
      }
    }
  )json");

  auto res = controller.trigger(R"(
    {
      "a": ["first", "second"]
    }
  )");

  CHECK(res[processors::JoltTransformJSON::Failure].size() == 0);
  CHECK(res[processors::JoltTransformJSON::Success].size() == 1);

  auto content = controller.plan->getContent(res.at(processors::JoltTransformJSON::Success).at(0));

  utils::verifyJSON(content, R"json(
    {
      "a_0": "first",
      "a_1": "second"
    }
  )json", true);
}

TEST_CASE("Shiftr put into array at index") {
  SingleProcessorTestController controller{std::make_unique<minifi::processors::JoltTransformJSON>("JoltProc")};
  auto proc = controller.getProcessor();
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltTransform, magic_enum::enum_name(processors::jolt_transform_json::JoltTransform::Shift));
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltSpecification, R"json(
    {
      "a": "out[1]",
      "b": "out[2].inner",
      "*": "arr[&]"
    }
  )json");

  auto res = controller.trigger(R"(
    {
      "a": "a_val",
      "b": "b_val",
      "2": "2_val"
    }
  )");

  CHECK(res[processors::JoltTransformJSON::Failure].size() == 0);
  CHECK(res[processors::JoltTransformJSON::Success].size() == 1);

  auto content = controller.plan->getContent(res.at(processors::JoltTransformJSON::Success).at(0));

  utils::verifyJSON(content, R"json(
    {
      "out": [null, "a_val", {"inner": "b_val"}],
      "arr": [null, null, "2_val"]
    }
  )json", true);
}

TEST_CASE("Shiftr multiple patterns") {
  // this is an extension, so we can escape and match on a '|' character
  SingleProcessorTestController controller{std::make_unique<minifi::processors::JoltTransformJSON>("JoltProc")};
  auto proc = controller.getProcessor();
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltTransform, magic_enum::enum_name(processors::jolt_transform_json::JoltTransform::Shift));
  controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltSpecification, R"json(
    {
      "a|b": "out1",
      "b\\||c": "out2"
    }
  )json");

  auto res = controller.trigger(R"(
    {
      "a": 1,
      "b": 2,
      "b|": 3,
      "c": 4
    }
  )");

  CHECK(res[processors::JoltTransformJSON::Failure].size() == 0);
  CHECK(res[processors::JoltTransformJSON::Success].size() == 1);

  auto content = controller.plan->getContent(res.at(processors::JoltTransformJSON::Success).at(0));

  utils::verifyJSON(content, R"json(
    {
      "out1": [1, 2],
      "out2": [3, 4]
    }
  )json", true);
}

static std::string to_string(const rapidjson::Value& val) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
  val.Accept(writer);
  return std::string{buf.GetString(), buf.GetSize()};
}

std::pair<size_t, size_t> offsetToCursor(std::string_view str, size_t offset) {
  size_t line_number{1};
  size_t line_offset{1};
  gsl_Expects(offset < str.size());
  for (size_t idx = 0; idx < offset; ++idx) {
    if (str[idx] == '\n') {
      ++line_number;
      line_offset = 1;
    } else {
      ++line_offset;
    }
  }
  return {line_number, line_offset};
}

TEST_CASE("Run tests from https://github.com/bazaarvoice/jolt") {
  std::set<std::filesystem::path> test_files{std::filesystem::directory_iterator(JOLT_TESTS_DIR), std::filesystem::directory_iterator{}};
  for (auto& entry : test_files) {
    INFO(entry);
    std::ifstream file{entry, std::ios::binary};
    std::string file_content{std::istreambuf_iterator<char>(file), {}};
    rapidjson::Document doc;
    rapidjson::ParseResult parse_res = doc.Parse<rapidjson::kParseCommentsFlag>(file_content);
    if (!parse_res) {
      auto cursor = offsetToCursor(file_content, parse_res.Offset());
      throw std::logic_error(fmt::format("Error in test json '{}' at {}:{} : {}", entry.string(), cursor.first, cursor.second, rapidjson::GetParseError_En(parse_res.Code())));
    }

    SingleProcessorTestController controller{std::make_unique<minifi::processors::JoltTransformJSON>("JoltProc")};
    auto proc = controller.getProcessor();
    LogTestController::getInstance().setTrace<minifi::processors::JoltTransformJSON>();
    controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltTransform, magic_enum::enum_name(processors::jolt_transform_json::JoltTransform::Shift));
    controller.plan->setProperty(proc, processors::JoltTransformJSON::JoltSpecification, to_string(doc["spec"]));

    auto res = controller.trigger(to_string(doc["input"]));

    CHECK(res[processors::JoltTransformJSON::Failure].size() == 0);
    CHECK(res[processors::JoltTransformJSON::Success].size() == 1);

    auto content = controller.plan->getContent(res.at(processors::JoltTransformJSON::Success).at(0));

    INFO(content);

    utils::verifyJSON(content, to_string(doc["expected"]), true);
  }
}

}   // namespace org::apache::nifi::minifi::test

// NOLINTEND(readability-container-size-empty)
