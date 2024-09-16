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
#include <string>
#include <catch2/generators/catch_generators.hpp>

#include "unit/Catch.h"
#include "processors/LogAttribute.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestUtils.h"

using LogAttribute = org::apache::nifi::minifi::processors::LogAttribute;

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

TEST_CASE("LogAttribute logs payload", "[LogAttribute]") {
  SingleProcessorTestController controller{std::make_unique<LogAttribute>("log_attribute")};
  const auto log_attribute = controller.getProcessor();
  LogTestController::getInstance().setTrace<LogAttribute>();

  const auto [hexencode_payload, expected_payload] = GENERATE(
    std::make_tuple("false", "hello world"),
    std::make_tuple("true", "68656c6c6f20776f726c64"));

  REQUIRE(controller.plan->setProperty(log_attribute, LogAttribute::LogPayload, "true"));
  REQUIRE(controller.plan->setProperty(log_attribute, LogAttribute::HexencodePayload, hexencode_payload));

  controller.plan->scheduleProcessor(log_attribute);
  const auto result = controller.trigger("hello world", {{"eng", "apple"}, {"ger", "Apfel"}, {"fra", "pomme"}});
  CHECK(result.at(LogAttribute::Success).size() == 1);
  CHECK(LogTestController::getInstance().contains("--------------------------------------------------", 1s));
  CHECK(LogTestController::getInstance().contains("Size:11 Offset:0", 0s));
  CHECK(LogTestController::getInstance().contains("FlowFile Attributes Map Content", 0s));
  CHECK(LogTestController::getInstance().contains("key:eng value:apple", 0s));
  CHECK(LogTestController::getInstance().contains("key:ger value:Apfel", 0s));
  CHECK(LogTestController::getInstance().contains("key:fra value:pomme", 0s));

  CHECK(LogTestController::getInstance().contains(fmt::format("Payload:\n{}", expected_payload), 0s));
}

TEST_CASE("LogAttribute LogLevel and LogPrefix", "[LogAttribute]") {
  SingleProcessorTestController controller{std::make_unique<LogAttribute>("log_attribute")};
  const auto log_attribute = controller.getProcessor();
  LogTestController::getInstance().setTrace<LogAttribute>();

  const auto [log_level, log_prefix, expected_dash] = GENERATE(
    std::make_tuple(std::string_view("info"), std::string_view(""), std::string_view("--------------------------------------------------")),
    std::make_tuple("critical", "foo", "-----------------------foo------------------------"),
    std::make_tuple("debug", "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi mollis neque sit amet dui pretium sodales.",
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi mollis neque sit amet dui pretium sodales."),
    std::make_tuple("error", "", "--------------------------------------------------"));

  REQUIRE(controller.plan->setProperty(log_attribute, LogAttribute::LogLevel, log_level));
  CHECK(controller.plan->setProperty(log_attribute, LogAttribute::LogPrefix, log_prefix) == (!log_prefix.empty()));

  controller.plan->scheduleProcessor(log_attribute);
  const auto result = controller.trigger("hello world", {{"eng", "apple"}, {"ger", "Apfel"}, {"fra", "pomme"}});
  CHECK(result.at(LogAttribute::Success).size() == 1);
  CHECK(LogTestController::getInstance().contains(fmt::format("[org::apache::nifi::minifi::processors::LogAttribute] [{}] Logging for flow file\n{}", log_level, expected_dash), 1s));
  CHECK(LogTestController::getInstance().contains("key:fra value:pomme", 0s));
  CHECK(LogTestController::getInstance().contains("Size:11 Offset:0", 0s));
  CHECK(LogTestController::getInstance().contains("FlowFile Attributes Map Content", 0s));
  CHECK(LogTestController::getInstance().contains("key:eng value:apple", 0s));
  CHECK(LogTestController::getInstance().contains("key:ger value:Apfel", 0s));
  CHECK(LogTestController::getInstance().contains("key:fra value:pomme", 0s));
}

TEST_CASE("LogAttribute filtering attributes", "[LogAttribute]") {
  SingleProcessorTestController controller{std::make_unique<LogAttribute>("log_attribute")};
  const auto log_attribute = controller.getProcessor();
  LogTestController::getInstance().setTrace<LogAttribute>();

  std::string_view attrs_to_log;
  std::string_view attrs_to_ignore;
  auto expected_eng = true;
  auto expected_ger = true;
  auto expected_fra = true;

  SECTION("Default") {
  }

  SECTION("Ignore eng and fra") {
    attrs_to_ignore = "eng,fra";
    expected_eng = false;
    expected_fra = false;
  }

  SECTION("Log eng and fra") {
    attrs_to_log = "eng,fra";
    expected_ger = false;
  }

  SECTION("Log eng and fra, ignore fra") {
    attrs_to_log = "eng,fra";
    attrs_to_ignore = "fra";
    expected_fra = false;
    expected_ger = false;
  }

  CHECK(controller.plan->setProperty(log_attribute, LogAttribute::AttributesToLog, attrs_to_log) == !attrs_to_log.empty());
  CHECK(controller.plan->setProperty(log_attribute, LogAttribute::AttributesToIgnore, attrs_to_ignore) == !attrs_to_ignore.empty());


  controller.plan->scheduleProcessor(log_attribute);
  const auto result = controller.trigger("hello world", {{"eng", "apple"}, {"ger", "Apfel"}, {"fra", "pomme"}});
  CHECK(result.at(LogAttribute::Success).size() == 1);
  CHECK(LogTestController::getInstance().contains("--------------------------------------------------", 1s));
  CHECK(LogTestController::getInstance().contains("Size:11 Offset:0", 0s));
  CHECK(LogTestController::getInstance().contains("FlowFile Attributes Map Content", 0s));
  CHECK(LogTestController::getInstance().contains("key:eng value:apple", 0s) == expected_eng);
  CHECK(LogTestController::getInstance().contains("key:ger value:Apfel", 0s) == expected_ger);
  CHECK(LogTestController::getInstance().contains("key:fra value:pomme", 0s) == expected_fra);
}

TEST_CASE("LogAttribute batch test", "[LogAttribute]") {
  SingleProcessorTestController controller{std::make_unique<LogAttribute>("log_attribute")};
  const auto log_attribute = controller.getProcessor();

  const auto [flow_files_to_log, expected_success_flow_files] = GENERATE(
    std::make_tuple("0", 3U),
    std::make_tuple("1", 1U),
    std::make_tuple("2", 2U));

  REQUIRE(controller.plan->setProperty(log_attribute, LogAttribute::FlowFilesToLog, flow_files_to_log));

  controller.plan->scheduleProcessor(log_attribute);
  const auto results = controller.trigger({{"first", {{"foo_key", "first_value"}}}, {"second", {{"foo_key", "second_value"}}}, {"third", {{"foo_key", "third_value"}}}});
  CHECK(results.at(LogAttribute::Success).size() == expected_success_flow_files);
}
}  // namespace org::apache::nifi::minifi::test
