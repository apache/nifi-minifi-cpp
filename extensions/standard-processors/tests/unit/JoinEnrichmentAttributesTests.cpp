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
#include "JoinEnrichmentAttributes.h"
#include "unit/Catch.h"
#include "unit/ProcessorUtils.h"
#include "unit/SingleProcessorTestController.h"
#include "utils/EnrichmentUtils.h"

namespace org::apache::nifi::minifi::standard::test {
TEST_CASE("JoinEnrichmentAttributes input without appropriate attributes") {
  minifi::test::SingleProcessorTestController
      test_controller(minifi::test::utils::make_processor<JoinEnrichmentAttributes>("JoinEnrichmentAttributes"));
  const auto trigger_result = test_controller.trigger("test_content");
  CHECK(trigger_result.at(JoinEnrichmentAttributes::Invalid).size() == 1);
  CHECK(trigger_result.at(JoinEnrichmentAttributes::Original).empty());
  CHECK(trigger_result.at(JoinEnrichmentAttributes::TimeoutRelationship).empty());
  CHECK(trigger_result.at(JoinEnrichmentAttributes::Joined).empty());
}

TEST_CASE("JoinEnrichmentAttributes invalid role") {
  minifi::test::SingleProcessorTestController
      test_controller(minifi::test::utils::make_processor<JoinEnrichmentAttributes>("JoinEnrichmentAttributes"));
  const auto trigger_result = test_controller.trigger(minifi::test::InputFlowFileData{.content = "first",
      .attributes = {{std::string{ENRICHMENT_GROUP_ID}, "foo"}, {std::string{ENRICHMENT_ROLE}, "FIREBIRD"}}});
  CHECK(trigger_result.at(JoinEnrichmentAttributes::Invalid).size() == 1);
  CHECK(trigger_result.at(JoinEnrichmentAttributes::Original).empty());
  CHECK(trigger_result.at(JoinEnrichmentAttributes::TimeoutRelationship).empty());
  CHECK(trigger_result.at(JoinEnrichmentAttributes::Joined).empty());
}

TEST_CASE("JoinEnrichmentAttributes same id same role same session") {
  minifi::test::SingleProcessorTestController
      test_controller(minifi::test::utils::make_processor<JoinEnrichmentAttributes>("JoinEnrichmentAttributes"));
  const auto trigger = test_controller.trigger(
      {minifi::test::InputFlowFileData{.content = "first",
           .attributes = {{std::string{ENRICHMENT_GROUP_ID}, "foo"}, {std::string{ENRICHMENT_ROLE}, "ORIGINAL"}}},
          minifi::test::InputFlowFileData{.content = "second",
              .attributes = {{std::string{ENRICHMENT_GROUP_ID}, "foo"}, {std::string{ENRICHMENT_ROLE}, "ORIGINAL"}}}});
  CHECK(trigger.at(JoinEnrichmentAttributes::Invalid).size() == 2);
  CHECK(trigger.at(JoinEnrichmentAttributes::Original).empty());
  CHECK(trigger.at(JoinEnrichmentAttributes::TimeoutRelationship).empty());
  CHECK(trigger.at(JoinEnrichmentAttributes::Joined).empty());
}

TEST_CASE("JoinEnrichmentAttributes same id same role different session") {
  minifi::test::SingleProcessorTestController
      test_controller(minifi::test::utils::make_processor<JoinEnrichmentAttributes>("JoinEnrichmentAttributes"));
  const auto first_trigger = test_controller.trigger(minifi::test::InputFlowFileData{.content = "first",
      .attributes = {{std::string{ENRICHMENT_GROUP_ID}, "foo"}, {std::string{ENRICHMENT_ROLE}, "ORIGINAL"}}});
  // First trigger no output (it holds the original waiting for its pair)
  CHECK(std::ranges::all_of(first_trigger, [](const auto& res) -> bool { return res.second.empty(); }));

  const auto second_trigger = test_controller.trigger(minifi::test::InputFlowFileData{.content = "second",
      .attributes = {{std::string{ENRICHMENT_GROUP_ID}, "foo"}, {std::string{ENRICHMENT_ROLE}, "ORIGINAL"}}});
  CHECK(second_trigger.at(JoinEnrichmentAttributes::Invalid).size() == 2);
  CHECK(second_trigger.at(JoinEnrichmentAttributes::Original).empty());
  CHECK(second_trigger.at(JoinEnrichmentAttributes::TimeoutRelationship).empty());
  CHECK(second_trigger.at(JoinEnrichmentAttributes::Joined).empty());
}

TEST_CASE("JoinEnrichmentAttributes same id diff role different session") {
  minifi::test::SingleProcessorTestController
      test_controller(minifi::test::utils::make_processor<JoinEnrichmentAttributes>("JoinEnrichmentAttributes"));
  const auto first_trigger = test_controller.trigger(minifi::test::InputFlowFileData{.content = "first",
      .attributes = {{std::string{ENRICHMENT_GROUP_ID}, "foo"}, {std::string{ENRICHMENT_ROLE}, "ORIGINAL"}, {"first_attr", "1"}}});
  // First trigger no output (it holds the original waiting for its pair)
  CHECK(std::ranges::all_of(first_trigger, [](const auto& res) -> bool { return res.second.empty(); }));

  const auto second_trigger = test_controller.trigger(minifi::test::InputFlowFileData{.content = "second",
      .attributes = {{std::string{ENRICHMENT_GROUP_ID}, "foo"}, {std::string{ENRICHMENT_ROLE}, "ENRICHMENT"}, {"second_attr", "2"}}});
  CHECK(second_trigger.at(JoinEnrichmentAttributes::Original).size() == 2);
  CHECK(second_trigger.at(JoinEnrichmentAttributes::Invalid).empty());
  CHECK(second_trigger.at(JoinEnrichmentAttributes::TimeoutRelationship).empty());
  REQUIRE(second_trigger.at(JoinEnrichmentAttributes::Joined).size() == 1);

  const auto joined_content = test_controller.plan->getContent(second_trigger.at(JoinEnrichmentAttributes::Joined).at(0));
  const auto joined_attrs = second_trigger.at(JoinEnrichmentAttributes::Joined).at(0)->getAttributes();

  CHECK(joined_content == "first");
  CHECK(joined_attrs.at(std::string{ENRICHMENT_GROUP_ID}) == "foo");
  CHECK(joined_attrs.at(std::string{ENRICHMENT_ROLE}) == "JOINED");
  CHECK(joined_attrs.at("first_attr") == "1");
  CHECK(joined_attrs.at("second_attr") == "2");
}

TEST_CASE("JoinEnrichmentAttributes test timeout") {
  minifi::test::SingleProcessorTestController
      test_controller(minifi::test::utils::make_processor<JoinEnrichmentAttributes>("JoinEnrichmentAttributes"));
  const auto proc = test_controller.getProcessor();
  CHECK(test_controller.plan->setProperty(proc, JoinEnrichmentAttributes::TimeoutProperty.name, "1 ms"));

  const auto first_trigger = test_controller.trigger(minifi::test::InputFlowFileData{.content = "first",
      .attributes = {{std::string{ENRICHMENT_GROUP_ID}, "foo"}, {std::string{ENRICHMENT_ROLE}, "ORIGINAL"}, {"first_attr", "1"}}});
  // First trigger no output (it holds the original waiting for its pair)
  CHECK(std::ranges::all_of(first_trigger, [](const auto& res) -> bool { return res.second.empty(); }));

  std::this_thread::sleep_for(1ms);
  const auto second_trigger = test_controller.trigger(minifi::test::InputFlowFileData{.content = "second",
      .attributes = {{std::string{ENRICHMENT_GROUP_ID}, "bar"}, {std::string{ENRICHMENT_ROLE}, "ENRICHMENT"}, {"second_attr", "2"}}});
  CHECK(second_trigger.at(JoinEnrichmentAttributes::Original).empty());
  CHECK(second_trigger.at(JoinEnrichmentAttributes::Invalid).empty());
  CHECK(second_trigger.at(JoinEnrichmentAttributes::TimeoutRelationship).size() == 1);
  REQUIRE(second_trigger.at(JoinEnrichmentAttributes::Joined).empty());
}

TEST_CASE("JoinEnrichmentAttributes no max batch size") {
  minifi::test::SingleProcessorTestController test_controller(minifi::test::utils::make_processor<JoinEnrichmentAttributes>("JoinEnrichmentAttributes"));
  const auto trigger_result = test_controller.trigger({{.content = "one"}, {.content = "two"}, {.content = "three"}});
  REQUIRE(trigger_result.at(JoinEnrichmentAttributes::Invalid).size() == 3);
}

TEST_CASE("JoinEnrichmentAttributes max batch size 2") {
  minifi::test::SingleProcessorTestController test_controller(minifi::test::utils::make_processor<JoinEnrichmentAttributes>("JoinEnrichmentAttributes"));
  const auto proc = test_controller.getProcessor();
  CHECK(test_controller.plan->setProperty(proc, JoinEnrichmentAttributes::MaxBatchSize.name, "2"));
  const auto trigger_result = test_controller.trigger({{.content = "one"}, {.content = "two"}, {.content = "three"}});
  REQUIRE(trigger_result.at(JoinEnrichmentAttributes::Invalid).size() == 2);
}
}  // namespace org::apache::nifi::minifi::standard::test
