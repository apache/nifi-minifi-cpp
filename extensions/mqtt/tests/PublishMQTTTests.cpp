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

#include "unit/Catch.h"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "unit/TestBase.h"
#include "../processors/PublishMQTT.h"
#include "unit/SingleProcessorTestController.h"
#include "core/Resource.h"
#include "controllers/XMLRecordSetWriter.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class TestPublishMQTTProcessor : public minifi::processors::PublishMQTT {
 public:
  explicit TestPublishMQTTProcessor(std::string_view name, const minifi::utils::Identifier& uuid = {})
      : minifi::processors::PublishMQTT(name, uuid) {}

  void initializeClient() override {
  }

  bool sendMessage(const std::vector<std::byte>&, const std::string&, const std::string&, const std::shared_ptr<core::FlowFile>&) override {
    return true;
  }

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override {
    minifi::processors::PublishMQTT::onTriggerImpl(context, session);
  }
};

REGISTER_RESOURCE(TestPublishMQTTProcessor, Processor);

struct PublishMQTTTestFixture {
  PublishMQTTTestFixture()
      : test_controller_(std::make_unique<TestPublishMQTTProcessor>("TestPublishMQTTProcessor")),
        publish_mqtt_processor_(dynamic_cast<TestPublishMQTTProcessor*>(test_controller_.getProcessor())) {
    REQUIRE(publish_mqtt_processor_ != nullptr);
    LogTestController::getInstance().setDebug<minifi::processors::PublishMQTT>();
  }

  PublishMQTTTestFixture(PublishMQTTTestFixture&&) = delete;
  PublishMQTTTestFixture(const PublishMQTTTestFixture&) = delete;
  PublishMQTTTestFixture& operator=(PublishMQTTTestFixture&&) = delete;
  PublishMQTTTestFixture& operator=(const PublishMQTTTestFixture&) = delete;

  ~PublishMQTTTestFixture() {
    LogTestController::getInstance().reset();
  }

  SingleProcessorTestController test_controller_;
  TestPublishMQTTProcessor* publish_mqtt_processor_ = nullptr;
};

TEST_CASE_METHOD(PublishMQTTTestFixture, "PublishMQTTTest_EmptyTopic", "[publishMQTTTest]") {
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE_THROWS_WITH(test_controller_.plan->scheduleProcessor(publish_mqtt_processor_),
      Catch::Matchers::EndsWith("Process Schedule Operation: PublishMQTT: Topic is required"));
}

TEST_CASE_METHOD(PublishMQTTTestFixture, "PublishMQTTTest_EmptyBrokerURI", "[publishMQTTTest]") {
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::Topic.name, "mytopic"));
  REQUIRE_THROWS_WITH(test_controller_.plan->scheduleProcessor(publish_mqtt_processor_),
      Catch::Matchers::EndsWith("Expected valid value from \"TestPublishMQTTProcessor::Broker URI\", but got PropertyNotSet (Property Error:2)"));
}

TEST_CASE_METHOD(PublishMQTTTestFixture, "PublishMQTTTest_EmptyClientID_V_3_1_0", "[publishMQTTTest]") {
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::Topic.name, "mytopic"));
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::AbstractMQTTProcessor::MqttVersion.name, std::string{magic_enum::enum_name(minifi::processors::mqtt::MqttVersions::V_3_1_0)}));
  REQUIRE_THROWS_WITH(test_controller_.plan->scheduleProcessor(publish_mqtt_processor_),
      Catch::Matchers::EndsWith("MQTT 3.1.0 specification does not support empty client IDs"));
}

TEST_CASE_METHOD(PublishMQTTTestFixture, "PublishMQTTTest_EmptyClientID_V_3", "[publishMQTTTest]") {
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::Topic.name, "mytopic"));
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::MessageExpiryInterval.name, "60 sec"));
  REQUIRE_NOTHROW(test_controller_.plan->scheduleProcessor(publish_mqtt_processor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Message Expiry Intervals. Property is not used.", 1s));
}

TEST_CASE_METHOD(PublishMQTTTestFixture, "PublishMQTTTest_ContentType_V_3", "[publishMQTTTest]") {
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::Topic.name, "mytopic"));
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::ContentType.name, "text/plain"));
  REQUIRE_NOTHROW(test_controller_.plan->scheduleProcessor(publish_mqtt_processor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Content Types. Property is not used.", 1s));
}

TEST_CASE_METHOD(PublishMQTTTestFixture, "PublishMQTT can publish the number of in-flight messages as a metric") {
  const auto node = publish_mqtt_processor_->getResponseNode();

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

TEST_CASE_METHOD(PublishMQTTTestFixture, "Test sending XML message records", "[publishMQTTTest]") {
  test_controller_.plan->addController("JsonTreeReader", "JsonTreeReader");
  auto xml_writer = test_controller_.plan->addController("XMLRecordSetWriter", "XMLRecordSetWriter");
  REQUIRE(test_controller_.plan->setProperty(xml_writer, minifi::standard::XMLRecordSetWriter::NameOfRootTag.name, "root"));
  REQUIRE(test_controller_.plan->setProperty(xml_writer, minifi::standard::XMLRecordSetWriter::NameOfRecordTag.name, "record"));

  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::Topic.name, "mytopic"));
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::RecordReader.name, "JsonTreeReader"));
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::RecordWriter.name, "XMLRecordSetWriter"));

  const auto trigger_results = test_controller_.trigger(R"([{"element1": "value1"}, {"element2": "42"}])");
  CHECK(trigger_results.at(TestPublishMQTTProcessor::Success).size() == 2);
  const auto flow_file_1 = trigger_results.at(TestPublishMQTTProcessor::Success).at(0);

  auto string_content = test_controller_.plan->getContent(flow_file_1);
  CHECK(string_content == R"(<?xml version="1.0"?><root><record><element1>value1</element1></record></root>)");

  const auto flow_file_2 = trigger_results.at(TestPublishMQTTProcessor::Success).at(1);
  string_content = test_controller_.plan->getContent(flow_file_2);
  CHECK(string_content == R"(<?xml version="1.0"?><root><record><element2>42</element2></record></root>)");
}

TEST_CASE_METHOD(PublishMQTTTestFixture, "Test scheduling failure if non-existant recordset reader or writer is set", "[publishMQTTTest]") {
  test_controller_.plan->addController("XMLReader", "XMLReader");
  test_controller_.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::Topic.name, "mytopic"));
  SECTION("RecordReader is set to invalid controller service") {
    REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::RecordReader.name, "invalid_reader"));
    REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::RecordWriter.name, "JsonRecordSetWriter"));
    REQUIRE_THROWS_WITH(test_controller_.trigger(), Catch::Matchers::EndsWith("'Record Reader' property is set to invalid controller service 'invalid_reader'"));
  }

  SECTION("RecordWriter is set to invalid controller service") {
    REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::RecordReader.name, "XMLReader"));
    REQUIRE(publish_mqtt_processor_->setProperty(minifi::processors::PublishMQTT::RecordWriter.name, "invalid_writer"));
    REQUIRE_THROWS_WITH(test_controller_.trigger(), Catch::Matchers::EndsWith("'Record Writer' property is set to invalid controller service 'invalid_writer'"));
  }
}

}  // namespace org::apache::nifi::minifi::test
