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

#include "Catch.h"
#include "TestBase.h"
#include "../processors/PublishMQTT.h"

namespace {
struct Fixture {
  Fixture() {
    LogTestController::getInstance().setDebug<minifi::processors::PublishMQTT>();
    plan_ = testController_.createPlan();
    publishMqttProcessor_ = plan_->addProcessor("PublishMQTT", "publishMqttProcessor");
  }

  ~Fixture() {
    LogTestController::getInstance().reset();
  }

  TestController testController_;
  std::shared_ptr<TestPlan> plan_;
  std::shared_ptr<core::Processor> publishMqttProcessor_;
};
}  // namespace

TEST_CASE_METHOD(Fixture, "PublishMQTTTest_EmptyTopic", "[publishMQTTTest]") {
  publishMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");
  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(publishMqttProcessor_), Catch::EndsWith("Required property is empty: Topic"));
  LogTestController::getInstance().reset();
}

TEST_CASE_METHOD(Fixture, "PublishMQTTTest_EmptyBrokerURI", "[publishMQTTTest]") {
  publishMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::Topic, "mytopic");
  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(publishMqttProcessor_), Catch::EndsWith("Required property is empty: Broker URI"));
  LogTestController::getInstance().reset();
}
