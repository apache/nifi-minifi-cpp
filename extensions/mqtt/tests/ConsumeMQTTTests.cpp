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
#include "core/Resource.h"
#include "unit/SingleProcessorTestController.h"
#include "rapidjson/document.h"
#include "unit/ProcessorUtils.h"

namespace org::apache::nifi::minifi::test {
void verifyXmlJsonResult(const std::string& json_content, size_t expected_record_count, bool add_attributes_as_fields) {
  rapidjson::Document document;
  document.Parse(json_content.c_str());
  REQUIRE(document.IsArray());
  REQUIRE(document.GetArray().Size() == expected_record_count);
  for (size_t i = 0; i < expected_record_count; ++i) {
    auto& current_record = document[gsl::narrow<rapidjson::SizeType>(i)];
    REQUIRE(current_record.IsObject());
    REQUIRE(current_record.HasMember("int_value"));
    uint64_t int_result = current_record["int_value"].GetInt64();
    CHECK(int_result == 42);
    REQUIRE(current_record.HasMember("string_value"));
    std::string string_result = current_record["string_value"].GetString();
    CHECK(string_result == "test");

    if (add_attributes_as_fields) {
      string_result = current_record["_topic"].GetString();
      CHECK(string_result == "mytopic/segment/" + std::to_string(i));
      auto array = current_record["_topicSegments"].GetArray();
      CHECK(array.Size() == 3);
      string_result = array[0].GetString();
      CHECK(string_result == "mytopic");
      string_result = array[1].GetString();
      CHECK(string_result == "segment");
      string_result = array[2].GetString();
      CHECK(string_result == std::to_string(i));
      int_result = current_record["_qos"].GetInt64();
      CHECK(int_result == i);
      bool bool_result = current_record["_isDuplicate"].GetBool();
      if (i == 0) {
        CHECK_FALSE(bool_result);
      } else {
        CHECK(bool_result);
      }
      bool_result = current_record["_isRetained"].GetBool();
      if (i == 0) {
        CHECK_FALSE(bool_result);
      } else {
        CHECK(bool_result);
      }
    } else {
      CHECK_FALSE(current_record.HasMember("_topic"));
      CHECK_FALSE(current_record.HasMember("_qos"));
      CHECK_FALSE(current_record.HasMember("_isDuplicate"));
      CHECK_FALSE(current_record.HasMember("_isRetained"));
    }
  }
}

class TestConsumeMQTTProcessor : public minifi::processors::ConsumeMQTT {
 public:
  using SmartMessage = processors::AbstractMQTTProcessor::SmartMessage;
  using MQTTMessageDeleter = processors::AbstractMQTTProcessor::MQTTMessageDeleter;
  explicit TestConsumeMQTTProcessor(minifi::core::ProcessorMetadata metadata)
      : minifi::processors::ConsumeMQTT(std::move(metadata)) {}

  using ConsumeMQTT::enqueueReceivedMQTTMsg;

  void initializeClient() override {
  }

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override {
    minifi::processors::ConsumeMQTT::onTriggerImpl(context, session);
  }
};

REGISTER_RESOURCE(TestConsumeMQTTProcessor, Processor);

struct ConsumeMqttTestFixture {
  ConsumeMqttTestFixture()
      : test_controller_(utils::make_processor<TestConsumeMQTTProcessor>("TestConsumeMQTTProcessor")),
        consume_mqtt_processor_(test_controller_.getProcessor()) {
    REQUIRE(consume_mqtt_processor_ != nullptr);
    LogTestController::getInstance().setDebug<TestConsumeMQTTProcessor>();
  }

  ConsumeMqttTestFixture(ConsumeMqttTestFixture&&) = delete;
  ConsumeMqttTestFixture(const ConsumeMqttTestFixture&) = delete;
  ConsumeMqttTestFixture& operator=(ConsumeMqttTestFixture&&) = delete;
  ConsumeMqttTestFixture& operator=(const ConsumeMqttTestFixture&) = delete;

  ~ConsumeMqttTestFixture() {
    LogTestController::getInstance().reset();
  }

  SingleProcessorTestController test_controller_;
  core::Processor* consume_mqtt_processor_ = nullptr;
};

using namespace std::literals::chrono_literals;

TEST_CASE_METHOD(ConsumeMqttTestFixture, "ConsumeMQTTTest_EmptyTopic", "[consumeMQTTTest]") {
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE_THROWS_WITH(test_controller_.plan->scheduleProcessor(consume_mqtt_processor_),
      Catch::Matchers::EndsWith("Expected valid value from \"Topic\", but got PropertyNotSet (Property Error:2)"));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "ConsumeMQTTTest_EmptyBrokerURI", "[consumeMQTTTest]") {
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE_THROWS_WITH(test_controller_.plan->scheduleProcessor(consume_mqtt_processor_),
      Catch::Matchers::EndsWith("Expected valid value from \"Broker URI\", but got PropertyNotSet (Property Error:2)"));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "ConsumeMQTTTest_DurableSessionWithID", "[consumeMQTTTest]") {
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::ClientID.name, "subscriber"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::QoS.name, "1"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::CleanSession.name, "false"));

  REQUIRE_NOTHROW(test_controller_.plan->scheduleProcessor(consume_mqtt_processor_));
  REQUIRE_FALSE(LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
    "by the broker when QoS is less than 1 for durable (non-clean) sessions. Only subscriptions are preserved.", 0s));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "ConsumeMQTTTest_DurableSessionWithQoS0", "[consumeMQTTTest]") {
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::ClientID.name, "subscriber"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::QoS.name, "0"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::CleanSession.name, "false"));

  REQUIRE_NOTHROW(test_controller_.plan->scheduleProcessor(consume_mqtt_processor_));

  REQUIRE(LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
    "by the broker when QoS is less than 1 for durable (non-clean) sessions. Only subscriptions are preserved.", 1s));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "ConsumeMQTTTest_DurableSessionWithID_V_5", "[consumeMQTTTest]") {
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::ClientID.name, "subscriber"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::QoS.name, "1"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::MqttVersion.name,
    std::string{magic_enum::enum_name(minifi::processors::mqtt::MqttVersions::V_5_0)}));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::SessionExpiryInterval.name, "1 h"));

  REQUIRE_NOTHROW(test_controller_.plan->scheduleProcessor(consume_mqtt_processor_));
  REQUIRE_FALSE(LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
                                                          "by the broker when QoS is less than 1 for durable (Session Expiry Interval > 0) sessions. Only subscriptions are preserved.", 0s));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "ConsumeMQTTTest_DurableSessionWithQoS0_V_5", "[consumeMQTTTest]") {
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::ClientID.name, "subscriber"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::QoS.name, "0"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::MqttVersion.name,
    std::string{magic_enum::enum_name(minifi::processors::mqtt::MqttVersions::V_5_0)}));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::SessionExpiryInterval.name, "1 h"));

  REQUIRE_NOTHROW(test_controller_.plan->scheduleProcessor(consume_mqtt_processor_));

  REQUIRE(LogTestController::getInstance().contains("[warning] Messages are not preserved during client disconnection "
                                                    "by the broker when QoS is less than 1 for durable (Session Expiry Interval > 0) sessions. Only subscriptions are preserved.", 1s));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "ConsumeMQTTTest_CleanStart_V_3", "[consumeMQTTTest]") {
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::CleanStart.name, "true"));

  REQUIRE_NOTHROW(test_controller_.plan->scheduleProcessor(consume_mqtt_processor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Clean Start. Property is not used.", 1s));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "ConsumeMQTTTest_SessionExpiryInterval_V_3", "[consumeMQTTTest]") {
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::SessionExpiryInterval.name, "1 h"));

  REQUIRE_NOTHROW(test_controller_.plan->scheduleProcessor(consume_mqtt_processor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Session Expiry Intervals. Property is not used.", 1s));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "ConsumeMQTTTest_CleanSession_V_5", "[consumeMQTTTest]") {
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::MqttVersion.name,
    std::string{magic_enum::enum_name(minifi::processors::mqtt::MqttVersions::V_5_0)}));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::SessionExpiryInterval.name, "0 s"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::CleanSession.name, "true"));

  REQUIRE_NOTHROW(test_controller_.plan->scheduleProcessor(consume_mqtt_processor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 5.0 specification does not support Clean Session. Property is not used.", 1s));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "ConsumeMQTTTest_TopicAliasMaximum_V_3", "[consumeMQTTTest]") {
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::TopicAliasMaximum.name, "1"));

  REQUIRE_NOTHROW(test_controller_.plan->scheduleProcessor(consume_mqtt_processor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Topic Alias Maximum. Property is not used.", 1s));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "ConsumeMQTTTest_ReceiveMaximum_V_3", "[consumeMQTTTest]") {
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::ReceiveMaximum.name, "1"));

  REQUIRE_NOTHROW(test_controller_.plan->scheduleProcessor(consume_mqtt_processor_));
  REQUIRE(LogTestController::getInstance().contains("[warning] MQTT 3.x specification does not support Receive Maximum. Property is not used.", 1s));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "Read XML messages and write them to json records", "[consumeMQTTTest]") {
  test_controller_.plan->addController("XMLReader", "XMLReader");
  test_controller_.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::RecordReader.name, "XMLReader"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::RecordWriter.name, "JsonRecordSetWriter"));

  bool add_attributes_as_fields = true;
  SECTION("Add attributes as fields by default") {
  }

  SECTION("Do not add attributes as fields") {
    add_attributes_as_fields = false;
    REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::AddAttributesAsFields.name, "false"));
  }

  const size_t expected_record_count = 2;
  const std::string payload = R"(<root><int_value>42</int_value><string_value>test</string_value></root>)";
  for (size_t i = 0; i < expected_record_count; ++i) {
    TestConsumeMQTTProcessor::SmartMessage message{std::unique_ptr<MQTTAsync_message, TestConsumeMQTTProcessor::MQTTMessageDeleter>(
        new MQTTAsync_message{.struct_id = {'M', 'Q', 'T', 'M'}, .struct_version = gsl::narrow<int>(i), .payloadlen = gsl::narrow<int>(payload.size()),
                              .payload = const_cast<char*>(payload.data()), .qos = gsl::narrow<int>(i), .retained = gsl::narrow<int>(i), .dup = gsl::narrow<int>(i),
                              .msgid = gsl::narrow<int>(i + 1), .properties = {}}),
      std::string{"mytopic/segment/" + std::to_string(i)}};  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    auto& test_processor = dynamic_cast<TestConsumeMQTTProcessor&>(consume_mqtt_processor_->getImpl());
    test_processor.enqueueReceivedMQTTMsg(std::move(message));
  }
  const auto trigger_results = test_controller_.trigger();
  CHECK(trigger_results.at(TestConsumeMQTTProcessor::Success).size() == 1);
  const auto flow_file = trigger_results.at(TestConsumeMQTTProcessor::Success).at(0);

  auto string_content = test_controller_.plan->getContent(flow_file);
  verifyXmlJsonResult(string_content, expected_record_count, add_attributes_as_fields);

  CHECK(*flow_file->getAttribute("record.count") == "2");
  CHECK(*flow_file->getAttribute("mqtt.broker") == "127.0.0.1:1883");
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "Invalid XML payload does not result in new flow files", "[consumeMQTTTest]") {
  test_controller_.plan->addController("XMLReader", "XMLReader");
  test_controller_.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::RecordReader.name, "XMLReader"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::RecordWriter.name, "JsonRecordSetWriter"));

  const std::string payload = "invalid xml payload";
  TestConsumeMQTTProcessor::SmartMessage message{
    std::unique_ptr<MQTTAsync_message, TestConsumeMQTTProcessor::MQTTMessageDeleter>(
      new MQTTAsync_message{.struct_id = {'M', 'Q', 'T', 'M'}, .struct_version = 1, .payloadlen = gsl::narrow<int>(payload.size()),
                            .payload = const_cast<char*>(payload.data()), .qos = 1, .retained = 0, .dup = 0, .msgid = 42, .properties = {}}),
    std::string{"mytopic"}};  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
  auto& test_processor = dynamic_cast<TestConsumeMQTTProcessor&>(consume_mqtt_processor_->getImpl());
  test_processor.enqueueReceivedMQTTMsg(std::move(message));

  const auto trigger_results = test_controller_.trigger();
  CHECK(trigger_results.at(TestConsumeMQTTProcessor::Success).empty());
  REQUIRE(LogTestController::getInstance().contains("[error] Failed to read records from MQTT message", 1s));
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "Read MQTT message and write it to a flow file", "[consumeMQTTTest]") {
  std::vector<std::string> expected_topic_segments;
  std::string topic;

  SECTION("Single topic segment") {
    expected_topic_segments = {"mytopic"};
    topic = "mytopic";
  }

  SECTION("Multiple topic segments") {
    expected_topic_segments = {"my", "topic", "segment"};
    topic = "my/topic/segment";
  }

  SECTION("Empty topic segment") {
    expected_topic_segments = {"mytopic", "", "segment"};
    topic = "mytopic//segment";
  }

  SECTION("Empty topic segment at the end") {
    expected_topic_segments = {"mytopic", ""};
    topic = "mytopic/";
  }

  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));

  const size_t expected_flow_file_count = 2;
  const std::string payload = "test MQTT payload";
  for (size_t i = 0; i < expected_flow_file_count; ++i) {
    TestConsumeMQTTProcessor::SmartMessage message{std::unique_ptr<MQTTAsync_message, TestConsumeMQTTProcessor::MQTTMessageDeleter>(
        new MQTTAsync_message{.struct_id = {'M', 'Q', 'T', 'M'}, .struct_version = 1, .payloadlen = gsl::narrow<int>(payload.size()),
                              .payload = const_cast<char*>(payload.data()), .qos = 1, .retained = 0, .dup = 0, .msgid = 42, .properties = {}}),
      std::string{topic}};  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    auto& test_processor = dynamic_cast<TestConsumeMQTTProcessor&>(consume_mqtt_processor_->getImpl());
    test_processor.enqueueReceivedMQTTMsg(std::move(message));
  }
  const auto trigger_results = test_controller_.trigger();
  CHECK(trigger_results.at(TestConsumeMQTTProcessor::Success).size() == expected_flow_file_count);
  for (size_t i = 0; i < expected_flow_file_count; ++i) {
    const auto flow_file = trigger_results.at(TestConsumeMQTTProcessor::Success).at(i);
    auto string_content = test_controller_.plan->getContent(flow_file);
    CHECK(string_content == payload);

    CHECK(*flow_file->getAttribute("mqtt.broker") == "127.0.0.1:1883");
    CHECK(*flow_file->getAttribute("mqtt.topic") == topic);
    for (size_t j = 0; j < expected_topic_segments.size(); ++j) {
      CHECK(*flow_file->getAttribute("mqtt.topic.segment." + std::to_string(j)) == expected_topic_segments[j]);
    }
    CHECK(*flow_file->getAttribute("mqtt.qos") == "1");
    CHECK(*flow_file->getAttribute("mqtt.isDuplicate") == "false");
    CHECK(*flow_file->getAttribute("mqtt.isRetained") == "false");
  }
}

TEST_CASE_METHOD(ConsumeMqttTestFixture, "Test scheduling failure if non-existent recordset reader or writer is set", "[consumeMQTTTest]") {
  test_controller_.plan->addController("XMLReader", "XMLReader");
  test_controller_.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::AbstractMQTTProcessor::BrokerURI.name, "127.0.0.1:1883"));
  REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::Topic.name, "mytopic"));
  SECTION("RecordReader is set to invalid controller service") {
    REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::RecordReader.name, "invalid_reader"));
    REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::RecordWriter.name, "JsonRecordSetWriter"));
    REQUIRE_THROWS_WITH(test_controller_.trigger(), Catch::Matchers::EndsWith("Controller service 'Record Reader' = 'invalid_reader' not found"));
  }

  SECTION("RecordWriter is set to invalid controller service") {
    REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::RecordReader.name, "XMLReader"));
    REQUIRE(test_controller_.plan->setProperty(consume_mqtt_processor_, minifi::processors::ConsumeMQTT::RecordWriter.name, "invalid_writer"));
    REQUIRE_THROWS_WITH(test_controller_.trigger(), Catch::Matchers::EndsWith("Controller service 'Record Writer' = 'invalid_writer' not found"));
  }
}

}  // namespace org::apache::nifi::minifi::test
