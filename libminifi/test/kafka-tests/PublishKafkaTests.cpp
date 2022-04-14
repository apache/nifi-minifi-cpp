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
#include <memory>

#include "../TestBase.h"
#include "../Catch.h"
#include "PublishKafka.h"
#include "SingleInputTestController.h"

namespace {

class PublishKafkaTestFixture {
 public:
  PublishKafkaTestFixture();

 protected:
  std::shared_ptr<minifi::processors::PublishKafka> publish_kafka_processor_;
  std::shared_ptr<minifi::test::SingleInputTestController> test_controller_;
};

PublishKafkaTestFixture::PublishKafkaTestFixture()
  : publish_kafka_processor_(std::make_shared<minifi::processors::PublishKafka>("PublishKafka")),
    test_controller_(std::make_shared<minifi::test::SingleInputTestController>(publish_kafka_processor_)) {
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::PublishKafka>();
}

TEST_CASE_METHOD(PublishKafkaTestFixture, "Scheduling should fail when batch size is larger than the max queue message count", "[testPublishKafka]") {
  publish_kafka_processor_->setProperty(org::apache::nifi::minifi::processors::PublishKafka::ClientName, "test_client");
  publish_kafka_processor_->setProperty(org::apache::nifi::minifi::processors::PublishKafka::SeedBrokers, "test_seedbroker");
  publish_kafka_processor_->setProperty(org::apache::nifi::minifi::processors::PublishKafka::QueueBufferMaxMessage, "1000");
  publish_kafka_processor_->setProperty(org::apache::nifi::minifi::processors::PublishKafka::BatchSize, "1500");
  REQUIRE_THROWS_WITH(test_controller_->trigger(""), "Process Schedule Operation: Invalid configuration: Batch Size cannot be larger than Queue Max Message");
}

}  // namespace
