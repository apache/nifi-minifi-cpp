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
#include "ConsumeKafka.h"
#include "MockLogger.h"
#include "MockProcessContext.h"
#include "MockProcessSession.h"
#include "MockUtils.h"
#include "catch2/catch_test_macros.hpp"

namespace org::apache::nifi::minifi::processors::test {

TEST_CASE("Schedule succeeds with valid required properties", "[ConsumeKafka]") {
  auto consume_kafka = ConsumeKafka(mock::getMockMetadata());
  auto context = mock::MockProcessContext{};
  context.properties_.emplace(ConsumeKafka::KafkaBrokers.name, "kafka-1:9092,kafka-2:9092");
  context.properties_.emplace(ConsumeKafka::TopicNames.name, "consume-test-topic");
  context.properties_.emplace(ConsumeKafka::GroupID.name, "test-consumer-group");
  // Defaults for the other required properties are supplied by ConsumeKafka::Properties.
  REQUIRE_NOTHROW(consume_kafka.onScheduleImpl(context));
}

// The receive-provenance path itself (transit URI shape "kafka://<brokers>/<topic>") is
// covered end-to-end by extensions/atlas/tests/features/kafka_topic_extractor.feature - it
// requires a real broker and cannot be reached from a mocked unit test without piping a
// fabricated rd_kafka_message_t through librdkafka, which the mock harness does not model.

}  // namespace org::apache::nifi::minifi::processors::test
