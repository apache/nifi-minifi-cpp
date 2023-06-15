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

#include "range/v3/algorithm/find_if.hpp"

#include "Catch.h"
#include "TestBase.h"
#include "../processors/PublishMQTT.h"

using namespace std::literals::chrono_literals;

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
}

TEST_CASE_METHOD(Fixture, "PublishMQTTTest_EmptyBrokerURI", "[publishMQTTTest]") {
  publishMqttProcessor_->setProperty(minifi::processors::PublishMQTT::Topic, "mytopic");
  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(publishMqttProcessor_), Catch::EndsWith("Required property is empty: Broker URI"));
}

TEST_CASE_METHOD(Fixture, "PublishMQTTTest_EmptyClientID_V_3_1_0", "[publishMQTTTest]") {
  publishMqttProcessor_->setProperty(minifi::processors::PublishMQTT::Topic, "mytopic");
  publishMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");
  publishMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::MqttVersion, toString(minifi::processors::mqtt::MqttVersions::V_3_1_0));
  REQUIRE_THROWS_WITH(plan_->scheduleProcessor(publishMqttProcessor_), Catch::EndsWith("MQTT 3.1.0 specification does not support empty client IDs"));
}

TEST_CASE_METHOD(Fixture, "PublishMQTTTest_EmptyClientID_V_3", "[publishMQTTTest]") {
  publishMqttProcessor_->setProperty(minifi::processors::PublishMQTT::Topic, "mytopic");
  publishMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");
  publishMqttProcessor_->setProperty(minifi::processors::PublishMQTT::MessageExpiryInterval, "60 sec");
  REQUIRE_NOTHROW(plan_->scheduleProcessor(publishMqttProcessor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Message Expiry Intervals. Property is not used.", 1s));
}

TEST_CASE_METHOD(Fixture, "PublishMQTTTest_ContentType_V_3", "[publishMQTTTest]") {
  publishMqttProcessor_->setProperty(minifi::processors::PublishMQTT::Topic, "mytopic");
  publishMqttProcessor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI, "127.0.0.1:1883");
  publishMqttProcessor_->setProperty(minifi::processors::PublishMQTT::ContentType, "text/plain");
  REQUIRE_NOTHROW(plan_->scheduleProcessor(publishMqttProcessor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Content Types. Property is not used.", 1s));
}

TEST_CASE_METHOD(Fixture, "PublishMQTT can publish the number of in-flight messages as a metric") {
  const auto node = publishMqttProcessor_->getResponseNode();

  SECTION("heartbeat metric") {
    const auto serialized_nodes = minifi::state::response::ResponseNode::serializeAndMergeResponseNodes({node});
    REQUIRE_FALSE(serialized_nodes.empty());
    const auto it = ranges::find_if(serialized_nodes[0].children, [](const auto& metric) { return metric.name == "InFlightMessageCount"; });
    REQUIRE(it != serialized_nodes[0].children.end());
    CHECK(it->value == "0");
  }

  SECTION("Prometheus metric") {
    const auto metrics = node->calculateMetrics();
    const auto it = ranges::find_if(metrics, [](const auto& metric) { return metric.name == "in_flight_message_count"; });
    REQUIRE(it != metrics.end());
    CHECK(it->value == 0.0);
  }
}
