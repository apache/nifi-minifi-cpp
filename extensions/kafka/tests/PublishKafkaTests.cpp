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
#include "PublishKafka.h"
#include "unit/SingleProcessorTestController.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Scheduling should fail when batch size is larger than the max queue message count", "[testPublishKafka]") {
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::PublishKafka>();
  SingleProcessorTestController test_controller(std::make_unique<processors::PublishKafka>("PublishKafka"));
  const auto publish_kafka = test_controller.getProcessor();
  publish_kafka->setProperty(processors::PublishKafka::ClientName.name, "test_client");
  publish_kafka->setProperty(processors::PublishKafka::SeedBrokers.name, "test_seedbroker");
  publish_kafka->setProperty(processors::PublishKafka::QueueBufferMaxMessage.name, "1000");
  publish_kafka->setProperty(processors::PublishKafka::BatchSize.name, "1500");
  REQUIRE_THROWS_WITH(test_controller.trigger(""), "Process Schedule Operation: Invalid configuration: Batch Size cannot be larger than Queue Max Message");
}

TEST_CASE("Compress Codec property") {
  using processors::PublishKafka;
  SingleProcessorTestController test_controller(std::make_unique<PublishKafka>("PublishKafka"));
  test_controller.getProcessor<PublishKafka>()->setProperty(PublishKafka::ClientName.name, "test_client");
  test_controller.getProcessor<PublishKafka>()->setProperty(PublishKafka::SeedBrokers.name, "test_seedbroker");
  test_controller.getProcessor<PublishKafka>()->setProperty(PublishKafka::Topic.name, "test_topic");
  test_controller.getProcessor<PublishKafka>()->setProperty(PublishKafka::MessageTimeOut.name, "10ms");

  SECTION("none") {
    REQUIRE(test_controller.getProcessor<PublishKafka>()->setProperty(PublishKafka::CompressCodec.name, "none"));
    REQUIRE_NOTHROW(test_controller.trigger("input"));
  }
  SECTION("gzip") {
    REQUIRE(test_controller.getProcessor<PublishKafka>()->setProperty(PublishKafka::CompressCodec.name, "gzip"));
    REQUIRE_NOTHROW(test_controller.trigger("input"));
  }
  SECTION("snappy") {
    REQUIRE(test_controller.getProcessor<PublishKafka>()->setProperty(PublishKafka::CompressCodec.name, "snappy"));
    REQUIRE_NOTHROW(test_controller.trigger("input"));
  }
  SECTION("lz4") {
    REQUIRE(test_controller.getProcessor<PublishKafka>()->setProperty(PublishKafka::CompressCodec.name, "lz4"));
    REQUIRE_NOTHROW(test_controller.trigger("input"));
  }
  SECTION("zstd") {
    REQUIRE(test_controller.getProcessor<PublishKafka>()->setProperty(PublishKafka::CompressCodec.name, "zstd"));
    REQUIRE_NOTHROW(test_controller.trigger("input"));
  }
  SECTION("foo") {
    REQUIRE_FALSE(test_controller.getProcessor<PublishKafka>()->setProperty(PublishKafka::CompressCodec.name, "foo"));
  }
}

}  // namespace org::apache::nifi::minifi::test
