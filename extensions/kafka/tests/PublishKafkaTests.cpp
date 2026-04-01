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
#include "MockLogger.h"
#include "MockProcessContext.h"
#include "MockProcessSession.h"
#include "MockUtils.h"
#include "PublishKafka.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"

namespace org::apache::nifi::minifi::processors::test {

TEST_CASE("Batch Size cannot be larger than Queue Max Message", "[PublishKafka]") {
  auto publish_kafka = PublishKafka(mock::getMockMetadata());
  auto context = mock::MockProcessContext{};
  context.properties_.emplace(PublishKafka::ClientName.name, "test_client");
  context.properties_.emplace(PublishKafka::SeedBrokers.name, "test_seedbroker");
  context.properties_.emplace(PublishKafka::QueueBufferMaxMessage.name, "1000");
  context.properties_.emplace(PublishKafka::BatchSize.name, "1500");

  REQUIRE_THROWS_WITH(publish_kafka.onScheduleImpl(context),
      "Process Schedule Operation: Invalid configuration: Batch Size cannot be larger than Queue Max Message");
}

TEST_CASE("Trigger without valid broker", "[PublishKafka]") {
  const auto logger = std::make_shared<mock::MockLogger>();
  auto publish_kafka = PublishKafka(mock::metadataWithLogger(logger));
  auto context = mock::MockProcessContext{};
  context.properties_.emplace(PublishKafka::ClientName.name, "test_client");
  context.properties_.emplace(PublishKafka::SeedBrokers.name, "test_seedbroker");
  context.properties_.emplace(PublishKafka::Topic.name, "test_topic");
  context.properties_.emplace(PublishKafka::MessageTimeOut.name, "10 ms");


  REQUIRE_NOTHROW(publish_kafka.onScheduleImpl(context));
  auto session = mock::MockProcessSession{};
  session.addInputFlowFile(mock::MockFlowFileData("test_input_flow_file"));
  REQUIRE_NOTHROW(publish_kafka.onTriggerImpl(context, session));

  CHECK_FALSE(logger->logs_.empty());
}

}  // namespace org::apache::nifi::minifi::processors::test
