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

#include "unit/Catch.h"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "unit/TestBase.h"
#include "../processors/ConsumeMQTT.h"

namespace {
struct Fixture {
  Fixture() {
    LogTestController::getInstance().setDebug<minifi::processors::ConsumeMQTT>();
    plan_ = testController_.createPlan();
    consumeMqttProcessor_ = plan_->addProcessor("ConsumeMQTT", "consumeMqttProcessor");
  }

  Fixture(Fixture&&) = delete;
  Fixture(const Fixture&) = delete;
  Fixture& operator=(Fixture&&) = delete;
  Fixture& operator=(const Fixture&) = delete;

  ~Fixture() {
    LogTestController::getInstance().reset();
  }

  TestController testController_;
  std::shared_ptr<TestPlan> plan_;
  core::Processor* consumeMqttProcessor_ = nullptr;
};
}  // namespace

using namespace std::literals::chrono_literals;

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_EmptyTopic", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(consumeMqttProcessor_),
      Catch::Matchers::EndsWith("Expected valid value from \"consumeMqttProcessor::Topic\", but got MiNiFi Property Error Category:2 (PropertyNotSet)"));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_EmptyBrokerURI", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(consumeMqttProcessor_),
      Catch::Matchers::EndsWith("Expected valid value from \"consumeMqttProcessor::Broker URI\", but got MiNiFi Property Error Category:2 (PropertyNotSet)"));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_DurableSessionWithoutID", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::QoS.name, "1");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::CleanSession.name, "false");

  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(consumeMqttProcessor_),
    Catch::Matchers::EndsWith("Processor must have a Client ID for durable (non-clean) sessions"));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_DurableSessionWithoutID_V_5", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::QoS.name, "1");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::MqttVersion.name, std::string{magic_enum::enum_name(minifi::processors::mqtt::MqttVersions::V_5_0)});
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::SessionExpiryInterval.name, "1 h");

  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(consumeMqttProcessor_),
                      Catch::Matchers::EndsWith("Processor must have a Client ID for durable (Session Expiry Interval > 0) sessions"));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_DurableSessionWithID", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::ClientID.name, "subscriber");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::QoS.name, "1");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::CleanSession.name, "false");

  REQUIRE_NOTHROW(plan_->scheduleProcessor(consumeMqttProcessor_));
  REQUIRE_FALSE(LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
    "by the broker when QoS is less than 1 for durable (non-clean) sessions. Only subscriptions are preserved.", 1s));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_DurableSessionWithQoS0", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::ClientID.name, "subscriber");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::QoS.name, "0");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::CleanSession.name, "false");

  REQUIRE_NOTHROW(plan_->scheduleProcessor(consumeMqttProcessor_));

  REQUIRE(LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
    "by the broker when QoS is less than 1 for durable (non-clean) sessions. Only subscriptions are preserved.", 1s));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_DurableSessionWithID_V_5", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::ClientID.name, "subscriber");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::QoS.name, "1");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::MqttVersion.name, std::string{magic_enum::enum_name(minifi::processors::mqtt::MqttVersions::V_5_0)});
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::SessionExpiryInterval.name, "1 h");

  REQUIRE_NOTHROW(plan_->scheduleProcessor(consumeMqttProcessor_));
  REQUIRE_FALSE(LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
                                                          "by the broker when QoS is less than 1 for durable (Session Expiry Interval > 0) sessions. Only subscriptions are preserved.", 1s));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_DurableSessionWithQoS0_V_5", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::ClientID.name, "subscriber");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::QoS.name, "0");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::MqttVersion.name, std::string{magic_enum::enum_name(minifi::processors::mqtt::MqttVersions::V_5_0)});
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::SessionExpiryInterval.name, "1 h");

  REQUIRE_NOTHROW(plan_->scheduleProcessor(consumeMqttProcessor_));

  REQUIRE(LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
                                                    "by the broker when QoS is less than 1 for durable (Session Expiry Interval > 0) sessions. Only subscriptions are preserved.", 1s));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_EmptyClientID_V_3_1_0", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::MqttVersion.name, std::string{magic_enum::enum_name(minifi::processors::mqtt::MqttVersions::V_3_1_0)});
  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(consumeMqttProcessor_), Catch::Matchers::EndsWith("MQTT 3.1.0 specification does not support empty client IDs"));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_CleanStart_V_3", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::CleanStart.name, "true");

  REQUIRE_NOTHROW(plan_->scheduleProcessor(consumeMqttProcessor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Clean Start. Property is not used.", 1s));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_SessionExpiryInterval_V_3", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::SessionExpiryInterval.name, "1 h");

  REQUIRE_NOTHROW(plan_->scheduleProcessor(consumeMqttProcessor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Session Expiry Intervals. Property is not used.", 1s));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_CleanSession_V_5", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::MqttVersion.name, std::string{magic_enum::enum_name(minifi::processors::mqtt::MqttVersions::V_5_0)});
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::SessionExpiryInterval.name, "0 s");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::CleanSession.name, "true");

  REQUIRE_NOTHROW(plan_->scheduleProcessor(consumeMqttProcessor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 5.0 specification does not support Clean Session. Property is not used.", 1s));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_TopicAliasMaximum_V_3", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::TopicAliasMaximum.name, "1");

  REQUIRE_NOTHROW(plan_->scheduleProcessor(consumeMqttProcessor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Topic Alias Maximum. Property is not used.", 1s));
}

TEST_CASE_METHOD(Fixture, "ConsumeMQTTTest_ReceiveMaximum_V_3", "[consumeMQTTTest]") {
  consumeMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::Topic.name, "mytopic");
  consumeMqttProcessor_->setProperty(minifi::processors::ConsumeMQTT::ReceiveMaximum.name, "1");

  REQUIRE_NOTHROW(plan_->scheduleProcessor(consumeMqttProcessor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Receive Maximum. Property is not used.", 1s));
}
