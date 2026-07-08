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
#include "ForkEnrichment.h"
#include "unit/Catch.h"
#include "unit/ProcessorUtils.h"
#include "unit/SingleProcessorTestController.h"

namespace org::apache::nifi::minifi::standard::test {
TEST_CASE("Fork Enrichment processor works") {
  minifi::test::SingleProcessorTestController test_controller(minifi::test::utils::make_processor<ForkEnrichment>("ForkEnrichment"));
  const auto trigger_result = test_controller.trigger("test_content");
  REQUIRE(trigger_result.contains(ForkEnrichment::Original));
  REQUIRE(trigger_result.contains(ForkEnrichment::Enrichment));
  const auto original_results = trigger_result.at(ForkEnrichment::Original);
  const auto enrichment_results = trigger_result.at(ForkEnrichment::Enrichment);
  REQUIRE(original_results.size() == 1);
  REQUIRE(enrichment_results.size() == 1);

  const auto original_content = test_controller.plan->getContent(original_results.at(0));
  const auto enrichment_content = test_controller.plan->getContent(enrichment_results.at(0));

  CHECK(original_content == enrichment_content);
  CHECK(original_content == "test_content");

  CHECK(original_results.at(0)->getAttribute(ForkEnrichment::EnrichmentRole.name) == "ORIGINAL");
  CHECK(enrichment_results.at(0)->getAttribute(ForkEnrichment::EnrichmentRole.name) == "ENRICHMENT");

  CHECK(original_results.at(0)->getAttribute(ForkEnrichment::EnrichmentGroupId.name) ==
      enrichment_results.at(0)->getAttribute(ForkEnrichment::EnrichmentGroupId.name));
}

TEST_CASE("ForkEnrichment no max batch size") {
  minifi::test::SingleProcessorTestController test_controller(minifi::test::utils::make_processor<ForkEnrichment>("ForkEnrichment"));
  const auto trigger_result = test_controller.trigger({{.content = "one"}, {.content = "two"}, {.content = "three"}});
  const auto original_results = trigger_result.at(ForkEnrichment::Original);
  const auto enrichment_results = trigger_result.at(ForkEnrichment::Enrichment);
  REQUIRE(original_results.size() == 3);
  REQUIRE(enrichment_results.size() == 3);
}

TEST_CASE("ForkEnrichment max batch size 2") {
  minifi::test::SingleProcessorTestController test_controller(minifi::test::utils::make_processor<ForkEnrichment>("ForkEnrichment"));
  const auto proc = test_controller.getProcessor();
  CHECK(test_controller.plan->setProperty(proc, ForkEnrichment::MaxBatchSize.name, "2"));
  const auto trigger_result = test_controller.trigger({{.content = "one"}, {.content = "two"}, {.content = "three"}});
  const auto original_results = trigger_result.at(ForkEnrichment::Original);
  const auto enrichment_results = trigger_result.at(ForkEnrichment::Enrichment);
  REQUIRE(original_results.size() == 2);
  REQUIRE(enrichment_results.size() == 2);
}
}  // namespace org::apache::nifi::minifi::standard::test
