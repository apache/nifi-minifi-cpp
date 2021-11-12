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

#include "TestBase.h"
#include "../processors/ConsumeMQTT.h"

TEST_CASE("ConsumeMQTTTest_EmptyTopic", "[consumeMQTTTest]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::ConsumeMQTT>();
  std::shared_ptr<TestPlan> plan = testController.createPlan();

  auto consumeMqttProcessor = plan->addProcessor("ConsumeMQTT", "consumeMqttProcessor");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");

  REQUIRE_THROWS_WITH(plan->scheduleProcessor(consumeMqttProcessor), Catch::EndsWith("Required property is empty: Topic"));
  LogTestController::getInstance().reset();
}

TEST_CASE("ConsumeMQTTTest_EmptyBrokerURI", "[consumeMQTTTest]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::ConsumeMQTT>();
  std::shared_ptr<TestPlan> plan = testController.createPlan();

  auto consumeMqttProcessor = plan->addProcessor("ConsumeMQTT", "consumeMqttProcessor");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::Topic, "mytopic");

  REQUIRE_THROWS_WITH(plan->scheduleProcessor(consumeMqttProcessor), Catch::EndsWith("Required property is empty: Broker URI"));
  LogTestController::getInstance().reset();
}

TEST_CASE("ConsumeMQTTTest_DurableSessionWithoutID", "[consumeMQTTTest]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::ConsumeMQTT>();
  std::shared_ptr<TestPlan> plan = testController.createPlan();

  auto consumeMqttProcessor = plan->addProcessor("ConsumeMQTT", "consumeMqttProcessor");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::Topic, "mytopic");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::QoS, "1");
  consumeMqttProcessor->setProperty(minifi::processors::ConsumeMQTT::CleanSession, "false");

  REQUIRE_THROWS_WITH(plan->scheduleProcessor(consumeMqttProcessor), Catch::EndsWith("Processor must have a Client ID for durable (non-clean) sessions"));
  LogTestController::getInstance().reset();
}

TEST_CASE("ConsumeMQTTTest_DurableSessionWithID", "[consumeMQTTTest]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::ConsumeMQTT>();
  std::shared_ptr<TestPlan> plan = testController.createPlan();

  auto consumeMqttProcessor = plan->addProcessor("ConsumeMQTT", "consumeMqttProcessor");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::ClientID, "subscriber");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::Topic, "mytopic");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::QoS, "1");
  consumeMqttProcessor->setProperty(minifi::processors::ConsumeMQTT::CleanSession, "false");

  REQUIRE_NOTHROW(plan->scheduleProcessor(consumeMqttProcessor));
  REQUIRE(!LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
    "by the broker when QoS is less than 1 for durable (non-clean) sessions. Only subscriptions are preserved."));
  LogTestController::getInstance().reset();
}

TEST_CASE("ConsumeMQTTTest_DurableSessionWithQoS0", "[consumeMQTTTest]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::ConsumeMQTT>();
  std::shared_ptr<TestPlan> plan = testController.createPlan();

  auto consumeMqttProcessor = plan->addProcessor("ConsumeMQTT", "consumeMqttProcessor");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::ClientID, "subscriber");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::Topic, "mytopic");
  consumeMqttProcessor->setProperty(minifi::processors::AbstractMQTTProcessor::QoS, "0");
  consumeMqttProcessor->setProperty(minifi::processors::ConsumeMQTT::CleanSession, "false");

  REQUIRE_NOTHROW(plan->scheduleProcessor(consumeMqttProcessor));

  REQUIRE(LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
    "by the broker when QoS is less than 1 for durable (non-clean) sessions. Only subscriptions are preserved."));
  LogTestController::getInstance().reset();
}
