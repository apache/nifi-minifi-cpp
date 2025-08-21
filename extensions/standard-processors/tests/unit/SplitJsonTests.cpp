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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "processors/SplitJson.h"
#include "unit/TestUtils.h"
#include "unit/ProcessorUtils.h"

namespace org::apache::nifi::minifi::test {

class SplitJsonTestFixture {
 public:
  SplitJsonTestFixture() :
      controller_(utils::make_processor<processors::SplitJson>("SplitJson")),
      split_json_processor_(controller_.getProcessor()) {
    REQUIRE(split_json_processor_);
    LogTestController::getInstance().setTrace<processors::SplitJson>();
  }

 protected:
  void verifySuccessfulSplit(const std::string& input_json_content, const std::string& json_path_expression, const std::vector<std::string>& expected_split_contents) {
    REQUIRE(controller_.plan->setProperty(split_json_processor_, processors::SplitJson::JsonPathExpression, json_path_expression));

    auto result = controller_.trigger({{.content = input_json_content, .attributes = {{"filename", "unique_file_name"}}}});

    CHECK(result.at(processors::SplitJson::Failure).empty());
    REQUIRE(result.at(processors::SplitJson::Original).size() == 1);
    auto original_flow_file = result.at(processors::SplitJson::Original).at(0);
    CHECK(controller_.plan->getContent(original_flow_file) == input_json_content);
    auto original_fragment_id = original_flow_file->getAttribute(processors::SplitJson::FragmentIdentifier.name);
    CHECK_FALSE(original_fragment_id->empty());
    auto fragment_count = original_flow_file->getAttribute(processors::SplitJson::FragmentCount.name);
    CHECK(fragment_count.value() == std::to_string(expected_split_contents.size()));

    REQUIRE(result.at(processors::SplitJson::Split).size() == expected_split_contents.size());
    for (size_t i = 0; i < expected_split_contents.size(); ++i) {
      auto flow_file = result.at(processors::SplitJson::Split).at(i);
      CHECK(controller_.plan->getContent(flow_file) == expected_split_contents[i]);
      auto fragment_id = flow_file->getAttribute(processors::SplitJson::FragmentIdentifier.name);
      CHECK(fragment_id.value() == original_fragment_id.value());
      CHECK(flow_file->getAttribute(processors::SplitJson::FragmentCount.name).value() == std::to_string(expected_split_contents.size()));
      CHECK(flow_file->getAttribute(processors::SplitJson::FragmentIndex.name).value() == std::to_string(i));
      CHECK(flow_file->getAttribute(processors::SplitJson::SegmentOriginalFilename.name).value() == "unique_file_name");
      CHECK_FALSE(flow_file->getAttribute(core::SpecialFlowAttribute::FILENAME).value() == "unique_file_name");
    }
  }

  SingleProcessorTestController controller_;
  core::Processor* split_json_processor_;
};

TEST_CASE_METHOD(SplitJsonTestFixture, "Query fails with parsing issues", "[SplitJsonTests]") {
  ProcessorTriggerResult result;
  std::string error_log;
  REQUIRE(controller_.plan->setProperty(split_json_processor_, processors::SplitJson::JsonPathExpression, "invalid json path"));
  SECTION("Flow file content is empty") {
    result = controller_.trigger({{.content = ""}});
    error_log = "FlowFile content is empty, transferring to Failure relationship";
  }

  SECTION("Flow file content is invalid json") {
    result = controller_.trigger({{.content = "invalid json"}});
    error_log = "FlowFile content is not a valid JSON document, transferring to Failure relationship";
  }

  SECTION("Json Path expression is invalid") {
    result = controller_.trigger({{.content = "{}"}});
    error_log = "Invalid JSON path expression 'invalid json path' set in the 'JsonPath Expression' property:";
  }

  CHECK(result.at(processors::SplitJson::Original).empty());
  CHECK(result.at(processors::SplitJson::Split).empty());
  CHECK(result.at(processors::SplitJson::Failure).size() == 1);
  CHECK(utils::verifyLogLinePresenceInPollTime(1s, error_log));
}

TEST_CASE_METHOD(SplitJsonTestFixture, "Query does not match input JSON content", "[SplitJsonTests]") {
  REQUIRE(controller_.plan->setProperty(split_json_processor_, processors::SplitJson::JsonPathExpression, "$.email"));

  std::string input_json;
  SECTION("Flow file content does not contain the specified path") {
    input_json = R"({"name": "John"})";
  }

  SECTION("Flow file content null") {
    input_json = "null";
  }

  auto result = controller_.trigger({{.content = input_json}});

  CHECK(result.at(processors::SplitJson::Original).empty());
  CHECK(result.at(processors::SplitJson::Split).empty());
  CHECK(result.at(processors::SplitJson::Failure).size() == 1);
  CHECK(utils::verifyLogLinePresenceInPollTime(1s, "JSON Path expression '$.email' did not match the input flow file content, transferring to Failure relationship"));
}

TEST_CASE_METHOD(SplitJsonTestFixture, "Query returns non-array result", "[SplitJsonTests]") {
  REQUIRE(controller_.plan->setProperty(split_json_processor_, processors::SplitJson::JsonPathExpression, "$.name"));

  auto result = controller_.trigger({{.content = R"({"name": "John"})"}});

  CHECK(result.at(processors::SplitJson::Original).empty());
  CHECK(result.at(processors::SplitJson::Split).empty());
  CHECK(result.at(processors::SplitJson::Failure).size() == 1);
  CHECK(utils::verifyLogLinePresenceInPollTime(1s, "JSON Path expression '$.name' did not return an array, transferring to Failure relationship"));
}

TEST_CASE_METHOD(SplitJsonTestFixture, "Query returns a single array of scalars", "[SplitJsonTests]") {
  verifySuccessfulSplit(R"({"names": ["John", "Jane"]})", "$.names", {"John", "Jane"});
}

TEST_CASE_METHOD(SplitJsonTestFixture, "Query returns an multiple matches", "[SplitJsonTests]") {
  const std::string json_content = R"({"company": {"departments": [{"name": "Engineering", "employees": ["Alice", "Bob"]}, {"name": "Marketing", "employees": "Dave"}]}})";
  verifySuccessfulSplit(json_content, "$.company.departments[*].employees", {R"(["Alice","Bob"])", "Dave"});
}

TEST_CASE_METHOD(SplitJsonTestFixture, "Query returns an array of objects", "[SplitJsonTests]") {
  const std::string json_content = R"({"company": {"departments": [{"name": "Engineering", "employees": ["Alice", "Bob"]}, {"name": "Marketing", "employees": "Dave"}]}})";
  verifySuccessfulSplit(json_content, "$.company.departments[*]", {R"({"employees":["Alice","Bob"],"name":"Engineering"})", R"({"employees":"Dave","name":"Marketing"})"});
}

TEST_CASE_METHOD(SplitJsonTestFixture, "Query returns an array of scalars with null values", "[SplitJsonTests]") {
  const std::string json_content = R"({"fruits": ["Apple", null, "Banana", null, "Cherry"]})";
  SECTION("Null value representation is set to empty string") {
    REQUIRE(controller_.plan->setProperty(split_json_processor_, processors::SplitJson::NullValueRepresentation, "empty string"));
    verifySuccessfulSplit(json_content, "$.fruits", {"Apple", "", "Banana", "", "Cherry"});
  }

  SECTION("Null value representation is set to 'null' string") {
    REQUIRE(controller_.plan->setProperty(split_json_processor_, processors::SplitJson::NullValueRepresentation, "the string 'null'"));
    verifySuccessfulSplit(json_content, "$.fruits", {"Apple", "null", "Banana", "null", "Cherry"});
  }
}

}  // namespace org::apache::nifi::minifi::test
