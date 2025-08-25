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
#include "processors/EvaluateJsonPath.h"
#include "unit/TestUtils.h"
#include "unit/ProcessorUtils.h"

namespace org::apache::nifi::minifi::test {

class EvaluateJsonPathTestFixture {
 public:
  EvaluateJsonPathTestFixture() :
      controller_(utils::make_processor<processors::EvaluateJsonPath>("EvaluateJsonPath")),
      evaluate_json_path_processor_(controller_.getProcessor()) {
    REQUIRE(evaluate_json_path_processor_);
    LogTestController::getInstance().setTrace<processors::EvaluateJsonPath>();
  }

 protected:
  SingleProcessorTestController controller_;
  core::Processor* evaluate_json_path_processor_;
};

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "At least one dynamic property must be specified", "[EvaluateJsonPathTests]") {
  REQUIRE_THROWS_WITH(controller_.trigger({{.content = "foo"}}), "Process Schedule Operation: At least one dynamic property must be specified with a valid JSON path expression");
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "When destination is set to flowfile content only one dynamic property is allowed", "[EvaluateJsonPathTests]") {
  REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::Destination, "flowfile-content"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "attribute1", "value1"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "attribute2", "value2"));
  REQUIRE_THROWS_WITH(controller_.trigger({{.content = "foo"}}), "Process Schedule Operation: Only one dynamic property is allowed for JSON path when destination is set to flowfile-content");
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "Input flowfile has invalid JSON as content", "[EvaluateJsonPathTests]") {
  ProcessorTriggerResult result;
  std::string error_log;
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "attribute1", "value1"));
  SECTION("Flow file content is empty") {
    result = controller_.trigger({{.content = ""}});
    error_log = "FlowFile content is empty, transferring to Failure relationship";
  }

  SECTION("Flow file content is invalid json") {
    result = controller_.trigger({{.content = "invalid json"}});
    error_log = "FlowFile content is not a valid JSON document, transferring to Failure relationship";
  }

  CHECK(result.at(processors::EvaluateJsonPath::Matched).empty());
  CHECK(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  CHECK(result.at(processors::EvaluateJsonPath::Failure).size() == 1);
  CHECK(utils::verifyLogLinePresenceInPollTime(1s, error_log));
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "Dynamic property contains invalid JSON path expression", "[EvaluateJsonPathTests]") {
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "attribute", "1234"));

  auto result = controller_.trigger({{.content = "{}"}});

  REQUIRE(result.at(processors::EvaluateJsonPath::Matched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Failure).size() == 1);

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Failure).at(0);

  CHECK(controller_.plan->getContent(result_flow_file) == "{}");
  CHECK(utils::verifyLogLinePresenceInPollTime(0s, "Invalid JSON path expression '1234' found for attribute key 'attribute'"));
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "JSON paths are not found in content when destination is set to attribute", "[EvaluateJsonPathTests]") {
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "attribute1", "$.firstName"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "attribute2", "$.lastName"));

  std::map<std::string, std::string> expected_attributes = {
    {"attribute1", ""},
    {"attribute2", ""}
  };

  bool warn_path_not_found_behavior = false;
  bool expect_attributes = false;

  SECTION("Ignore path not found behavior") {
    REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::PathNotFoundBehavior, "ignore"));
    expect_attributes = true;
  }

  SECTION("Skip path not found behavior") {
    REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::PathNotFoundBehavior, "skip"));
  }

  SECTION("Warn path not found behavior") {
    REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::PathNotFoundBehavior, "warn"));
    warn_path_not_found_behavior = true;
    expect_attributes = true;
  }

  auto result = controller_.trigger({{.content = "{}"}});

  REQUIRE(result.at(processors::EvaluateJsonPath::Matched).size() == 1);
  REQUIRE(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Failure).empty());

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Matched).at(0);

  CHECK(controller_.plan->getContent(result_flow_file) == "{}");

  for (const auto& [key, value] : expected_attributes) {
    if (!expect_attributes) {
      CHECK_FALSE(result_flow_file->getAttribute(key));
    } else {
      CHECK(result_flow_file->getAttribute(key).value() == value);
    }
  }

  if (warn_path_not_found_behavior) {
    CHECK(utils::verifyLogLinePresenceInPollTime(0s, "JSON path '$.firstName' not found for attribute key 'attribute1'"));
    CHECK(utils::verifyLogLinePresenceInPollTime(0s, "JSON path '$.lastName' not found for attribute key 'attribute2'"));
  }
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "JSON paths are not found in content when destination is set in content", "[EvaluateJsonPathTests]") {
  REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::Destination, "flowfile-content"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "attribute", "$.firstName"));

  bool warn_path_not_found_behavior = false;
  SECTION("Ignore path not found behavior") {
    REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::PathNotFoundBehavior, "ignore"));
  }

  SECTION("Skip path not found behavior") {
    REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::PathNotFoundBehavior, "skip"));
  }

  SECTION("Warn path not found behavior") {
    REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::PathNotFoundBehavior, "warn"));
    warn_path_not_found_behavior = true;
  }

  auto result = controller_.trigger({{.content = "{}"}});

  REQUIRE(result.at(processors::EvaluateJsonPath::Matched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Unmatched).size() == 1);
  REQUIRE(result.at(processors::EvaluateJsonPath::Failure).empty());

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Unmatched).at(0);

  CHECK(controller_.plan->getContent(result_flow_file) == "{}");
  CHECK_FALSE(result_flow_file->getAttribute("attribute"));

  if (warn_path_not_found_behavior) {
    CHECK(utils::verifyLogLinePresenceInPollTime(0s, "JSON path '$.firstName' not found for attribute key 'attribute'"));
  }
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "JSON path query result does not match the required return type", "[EvaluateJsonPathTests]") {
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "attribute", "$.name"));

  SECTION("Return type is set to scalar automatically when destination is set to flowfile-attribute") {
    REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::Destination, "flowfile-attribute"));
  }

  SECTION("Return type is set to scalar with flowfile-content destination") {
    REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::ReturnType, "scalar"));
    REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::Destination, "flowfile-content"));
  }

  std::string json_content = R"({"name": {"firstName": "John", "lastName": "Doe"}})";
  auto result = controller_.trigger({{.content = json_content}});

  REQUIRE(result.at(processors::EvaluateJsonPath::Matched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Failure).size() == 1);

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Failure).at(0);

  CHECK(controller_.plan->getContent(result_flow_file) == json_content);
  CHECK_FALSE(result_flow_file->getAttribute("attribute"));
  CHECK(utils::verifyLogLinePresenceInPollTime(0s, "JSON path '$.name' returned a non-scalar value or multiple values for attribute key 'attribute', transferring to Failure relationship"));
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "Query JSON object and write it to flow file", "[EvaluateJsonPathTests]") {
  REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::Destination, "flowfile-content"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "jsonPath", "$.name"));

  std::string json_content = R"({"name": {"firstName": "John", "lastName": "Doe"}})";
  auto result = controller_.trigger({{.content = json_content}});

  REQUIRE(result.at(processors::EvaluateJsonPath::Matched).size() == 1);
  REQUIRE(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Failure).empty());

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Matched).at(0);

  CHECK(controller_.plan->getContent(result_flow_file) == R"({"firstName":"John","lastName":"Doe"})");
  CHECK_FALSE(result_flow_file->getAttribute("jsonPath"));
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "Query multiple scalars and write them to attributes", "[EvaluateJsonPathTests]") {
  REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::Destination, "flowfile-attribute"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "firstName", "$.name.firstName"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "lastName", "$.name.lastName"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "id", "$.id"));

  std::string json_content = R"({"id": 1234, "name": {"firstName": "John", "lastName": "Doe"}})";
  auto result = controller_.trigger({{.content = json_content}});

  REQUIRE(result.at(processors::EvaluateJsonPath::Matched).size() == 1);
  REQUIRE(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Failure).empty());

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Matched).at(0);

  CHECK(controller_.plan->getContent(result_flow_file) == json_content);
  CHECK(result_flow_file->getAttribute("firstName").value() == "John");
  CHECK(result_flow_file->getAttribute("lastName").value() == "Doe");
  CHECK(result_flow_file->getAttribute("id").value() == "1234");
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "Query a single scalar and write it to flow file", "[EvaluateJsonPathTests]") {
  REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::Destination, "flowfile-content"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "firstName", "$.name.firstName"));

  std::string json_content = R"({"id": 1234, "name": {"firstName": "John", "lastName": "Doe"}})";
  auto result = controller_.trigger({{.content = json_content}});

  REQUIRE(result.at(processors::EvaluateJsonPath::Matched).size() == 1);
  REQUIRE(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Failure).empty());

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Matched).at(0);

  CHECK(controller_.plan->getContent(result_flow_file) == "John");
  CHECK_FALSE(result_flow_file->getAttribute("firstName"));
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "Query has multiple results", "[EvaluateJsonPathTests]") {
  REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::Destination, "flowfile-content"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "firstName", "$.users[*].name.firstName"));

  std::string json_content = R"({"users": [{"id": 1234, "name": {"firstName": "John", "lastName": "Doe"}}, {"id": 2345, "name": {"firstName": "Jane", "lastName": "Smith"}}]})";
  auto result = controller_.trigger({{.content = json_content}});

  REQUIRE(result.at(processors::EvaluateJsonPath::Matched).size() == 1);
  REQUIRE(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Failure).empty());

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Matched).at(0);

  CHECK(controller_.plan->getContent(result_flow_file) == "[\"John\",\"Jane\"]");
  CHECK_FALSE(result_flow_file->getAttribute("firstName"));
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "Query result is null value in flow file content", "[EvaluateJsonPathTests]") {
  REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::Destination, "flowfile-content"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "email", "$.name.email"));

  std::string expected_content;
  SECTION("Null value representation is set to empty string") {
    expected_content = "";
  }

  SECTION("Null value representation is null string") {
    REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::NullValueRepresentation, "the string 'null'"));
    expected_content = "null";
  }

  std::string json_content = R"({"id": 1234, "name": {"firstName": "John", "lastName": "Doe", "email": null}})";
  auto result = controller_.trigger({{.content = json_content}});

  REQUIRE(result.at(processors::EvaluateJsonPath::Matched).size() == 1);
  REQUIRE(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Failure).empty());

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Matched).at(0);

  CHECK(controller_.plan->getContent(result_flow_file) == expected_content);
  CHECK_FALSE(result_flow_file->getAttribute("firstName"));
}

TEST_CASE_METHOD(EvaluateJsonPathTestFixture, "Query result is null value in flow file attribute", "[EvaluateJsonPathTests]") {
  REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::Destination, "flowfile-attribute"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "firstName", "$.user.firstName"));
  REQUIRE(controller_.plan->setDynamicProperty(evaluate_json_path_processor_, "email", "$.user.email"));

  std::string expected_null_value;
  SECTION("Null value representation is set to empty string") {
    expected_null_value = "";
  }

  SECTION("Null value representation is null string") {
    REQUIRE(controller_.plan->setProperty(evaluate_json_path_processor_, processors::EvaluateJsonPath::NullValueRepresentation, "the string 'null'"));
    expected_null_value = "null";
  }

  std::string json_content = R"({"id": 1234, "user": {"firstName": "John", "lastName": "Doe", "email": null}})";
  auto result = controller_.trigger({{.content = json_content}});

  REQUIRE(result.at(processors::EvaluateJsonPath::Matched).size() == 1);
  REQUIRE(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  REQUIRE(result.at(processors::EvaluateJsonPath::Failure).empty());

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Matched).at(0);

  CHECK(controller_.plan->getContent(result_flow_file) == json_content);
  CHECK(result_flow_file->getAttribute("firstName").value() == "John");
  CHECK(result_flow_file->getAttribute("email").value() == expected_null_value);
}

}  // namespace org::apache::nifi::minifi::test
