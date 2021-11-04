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
#include "../processors/ConsumeMQTT.h"

namespace {
struct Fixture {
  Fixture() {
    LogTestController::getInstance().setDebug<minifi::processors::ConsumeMQTT>();
    plan_ = testController_.createPlan();
    consumeMqttProcessor_ = plan_->addProcessor("ConsumeMQTT", "consumeMqttProcessor");
  }

  ~Fixture() {
    LogTestController::getInstance().reset();
  }

  TestController testController_;
  std::shared_ptr<TestPlan> plan_;
  std::shared_ptr<core::Processor> consumeMqttProcessor_;
};
}  // namespace

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_EmptyTopic", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");
  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(consumeMqttProcessor_), Catch::EndsWith("Required property is empty: Topic"));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_EmptyBrokerURI", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::Topic, "mytopic");
  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(consumeMqttProcessor_), Catch::EndsWith("Required property is empty: Broker URI"));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_DurableSessionWithoutID", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::Topic, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::QoS, "1");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::CleanSession, "false");

  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(consumeMqttProcessor_),
    Catch::EndsWith("Processor must have a Client ID for durable (non-clean) sessions"));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_DurableSessionWithID", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::ClientID, "subscriber");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::Topic, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::QoS, "1");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::CleanSession, "false");

  REQUIRE_NOTHROW(plan_->scheduleProcessor(consumeMqttProcessor_));
  REQUIRE_FALSE(LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
    "by the broker when QoS is less than 1 for durable (non-clean) sessions. Only subscriptions are preserved."));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_DurableSessionWithQoS0", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::ClientID, "subscriber");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::Topic, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::QoS, "0");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::CleanSession, "false");

  REQUIRE_NOTHROW(plan_->scheduleProcessor(consumeMqttProcessor_));

  REQUIRE(LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
    "by the broker when QoS is less than 1 for durable (non-clean) sessions. Only subscriptions are preserved."));
}
