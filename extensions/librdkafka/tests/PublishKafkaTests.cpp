/**
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

#include "librdkafka/PublishKafka.h"
#include "TestBase.h"

namespace {

class PublishKafkaTestRunner {
 public:
  PublishKafkaTestRunner();
  void setPublishKafkaProperty(const core::Property& property, const std::string& value);
  void setPublishKafkaDynamicProperty(const std::string& property, const std::string& value);
  std::shared_ptr<core::FlowFile> createFlowFile() const;
  void enqueueFlowFile(const std::shared_ptr<core::FlowFile>& flow_file);
  void runProcessor();

 private:
  TestController test_controller_;
  std::shared_ptr<TestPlan> test_plan_;
  std::shared_ptr<core::Processor> upstream_processor_;
  std::shared_ptr<core::Processor> publish_kafka_processor_;
  std::shared_ptr<minifi::Connection> incoming_connection_;
};

const core::Relationship Success{"success", ""};
const core::Relationship Failure{"failure", ""};

PublishKafkaTestRunner::PublishKafkaTestRunner() : test_plan_(test_controller_.createPlan()) {
  LogTestController::getInstance().reset();
  LogTestController::getInstance().setDebug<processors::PublishKafka>();

  upstream_processor_ = test_plan_->addProcessor("GenerateFlowFile", "generate_flow_file");
  publish_kafka_processor_ = test_plan_->addProcessor("PublishKafka", "publish_kafka");
  publish_kafka_processor_->setAutoTerminatedRelationships({Success, Failure});
  incoming_connection_ = test_plan_->addConnection(upstream_processor_, Success, publish_kafka_processor_);

  test_plan_->setProperty(publish_kafka_processor_, processors::PublishKafka::SeedBrokers.getName(), "localhost:9092");
  test_plan_->setProperty(publish_kafka_processor_, processors::PublishKafka::ClientName.getName(), "Default_client_name");
  test_plan_->setProperty(publish_kafka_processor_, processors::PublishKafka::Topic.getName(), "Default_topic");

  test_plan_->increment_location();  // current processor is generate_flow_file
  test_plan_->increment_location();  // current processor is publish_kafka
}

void PublishKafkaTestRunner::setPublishKafkaProperty(const core::Property& property, const std::string& value) {
  test_plan_->setProperty(publish_kafka_processor_, property.getName(), value);
}

void PublishKafkaTestRunner::setPublishKafkaDynamicProperty(const std::string& property, const std::string& value) {
  test_plan_->setProperty(publish_kafka_processor_, property, value, true);
}

std::shared_ptr<core::FlowFile> PublishKafkaTestRunner::createFlowFile() const {
  const auto context = test_plan_->getProcessContextForProcessor(upstream_processor_);
  const auto session_factory = std::make_shared<core::ProcessSessionFactory>(context);
  const auto session = session_factory->createSession();
  auto flow_file = session->create();

  const std::string data = "flow file content";
  minifi::io::BufferStream stream{reinterpret_cast<const uint8_t*>(data.c_str()), static_cast<unsigned int>(data.length())};
  session->importFrom(stream, flow_file);
  session->transfer(flow_file, Success);
  session->commit();

  return flow_file;
}

void PublishKafkaTestRunner::enqueueFlowFile(const std::shared_ptr<core::FlowFile>& flow_file) {
  incoming_connection_->put(flow_file);
}

void PublishKafkaTestRunner::runProcessor() {
  test_plan_->runCurrentProcessor();
}

}  // namespace

TEST_CASE("PublishKafka can set the Topic from an attribute", "[PublishKafka][properties][expression_language]") {
  PublishKafkaTestRunner test_runner;
  test_runner.setPublishKafkaProperty(processors::PublishKafka::Topic, "${kafka_topic}-111");

  const auto flow_file = test_runner.createFlowFile();
  flow_file->setAttribute("kafka_topic", "Kafka_topic_for_unit_test");
  test_runner.enqueueFlowFile(flow_file);

  test_runner.runProcessor();

  REQUIRE(LogTestController::getInstance().contains("PublishKafka: topic for flow file " + flow_file->getUUIDStr() + " is 'Kafka_topic_for_unit_test-111'"));
}

TEST_CASE("PublishKafka can set DeliveryGuarantee from an attribute", "[PublishKafka][properties][expression_language]") {
  PublishKafkaTestRunner test_runner;
  test_runner.setPublishKafkaProperty(processors::PublishKafka::DeliveryGuarantee, "${kafka_require_num_acks}");

  const auto flow_file = test_runner.createFlowFile();
  flow_file->setAttribute("kafka_require_num_acks", "7");
  test_runner.enqueueFlowFile(flow_file);

  test_runner.runProcessor();

  REQUIRE(LogTestController::getInstance().contains("PublishKafka: request.required.acks [7]"));
}

TEST_CASE("PublishKafka can set RequestTimeOut", "[PublishKafka][properties]") {
  PublishKafkaTestRunner test_runner;
  test_runner.setPublishKafkaProperty(processors::PublishKafka::RequestTimeOut, "123 s");

  const auto flow_file = test_runner.createFlowFile();
  test_runner.enqueueFlowFile(flow_file);

  test_runner.runProcessor();

  REQUIRE(LogTestController::getInstance().contains("PublishKafka: request.timeout.ms [123000]"));
}

TEST_CASE("PublishKafka can set MessageTimeOut", "[PublishKafka][properties]") {
  PublishKafkaTestRunner test_runner;
  test_runner.setPublishKafkaProperty(processors::PublishKafka::MessageTimeOut, "5 min");

  const auto flow_file = test_runner.createFlowFile();
  test_runner.enqueueFlowFile(flow_file);

  test_runner.runProcessor();

  REQUIRE(LogTestController::getInstance().contains("PublishKafka: message.timeout.ms [300000]"));
}

TEST_CASE("PublishKafka can set SeedBrokers with expression language", "[PublishKafka][properties][expression_language]") {
  PublishKafkaTestRunner test_runner;
  test_runner.setPublishKafkaProperty(processors::PublishKafka::SeedBrokers, "localhost:${literal(9000):plus(123)}");

  test_runner.runProcessor();

  REQUIRE(LogTestController::getInstance().contains("PublishKafka: bootstrap.servers [localhost:9123]"));
}

TEST_CASE("PublishKafka can set ClientName with expression language", "[PublishKafka][properties][expression_language]") {
  PublishKafkaTestRunner test_runner;
  test_runner.setPublishKafkaProperty(processors::PublishKafka::ClientName, "client_no_${literal(6):multiply(7)}");

  test_runner.runProcessor();

  REQUIRE(LogTestController::getInstance().contains("PublishKafka: client.id [client_no_42]"));
}

TEST_CASE("If KafkaKey is not set, then PublishKafka sets the message key to the flow file ID", "[PublishKafka][properties]") {
  PublishKafkaTestRunner test_runner;

  const auto flow_file = test_runner.createFlowFile();
  test_runner.enqueueFlowFile(flow_file);

  test_runner.runProcessor();

  REQUIRE(LogTestController::getInstance().contains("PublishKafka: Message Key [" + flow_file->getUUIDStr() + "]"));
}

TEST_CASE("PublishKafka can set KafkaKey from an attribute", "[PublishKafka][properties][expression_language]") {
  PublishKafkaTestRunner test_runner;
  test_runner.setPublishKafkaProperty(processors::PublishKafka::KafkaKey, "${kafka_message_key}");

  const auto flow_file = test_runner.createFlowFile();
  flow_file->setAttribute("kafka_message_key", "unique_message_key_123");
  test_runner.enqueueFlowFile(flow_file);

  test_runner.runProcessor();

  REQUIRE(LogTestController::getInstance().contains("PublishKafka: Message Key [unique_message_key_123]"));
}

TEST_CASE("PublishKafka dynamic properties can use expression language", "[PublishKafka][properties][expression_language]") {
  PublishKafkaTestRunner test_runner;
  test_runner.setPublishKafkaDynamicProperty("retry.backoff.ms", "${literal(3):multiply(60):multiply(1000)}");

  test_runner.runProcessor();

  REQUIRE(LogTestController::getInstance().contains("PublishKafka: DynamicProperty: [retry.backoff.ms] -> [180000]"));
}

TEST_CASE("PublishKafka complains if Message Key Field is set, but only if it is set", "[PublishKafka][properties]") {
  PublishKafkaTestRunner test_runner;

  SECTION("Message Key Field is not set, so there is no error") {
    test_runner.runProcessor();
    REQUIRE_FALSE(LogTestController::getInstance().contains("error"));
  }

  SECTION("Message Key Field is set, so there is an error log") {
    test_runner.setPublishKafkaProperty(processors::PublishKafka::MessageKeyField, "kafka.key");
    test_runner.runProcessor();
    REQUIRE(LogTestController::getInstance().contains("The " + processors::PublishKafka::MessageKeyField.getName() +
        " property is set. This property is DEPRECATED and has no effect"));
  }
}
