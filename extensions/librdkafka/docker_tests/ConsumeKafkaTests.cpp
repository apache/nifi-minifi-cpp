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

#define CATCH_CONFIG_MAIN

#include <memory>
#include <string>
#include <set>

// #include "TestBase.h"
#include "../../../libminifi/test/TestBase.h"

#include "../ConsumeKafka.h"
#include "utils/file/FileUtils.h"
#include "utils/OptionalUtils.h"
#include "utils/RegexUtils.h"
#include "utils/TestUtils.h"
#include "utils/IntegrationTestUtils.h"

namespace {

class ConsumeKafkaTest {
 public:
  using ConsumeKafka = org::apache::nifi::minifi::processors::ConsumeKafka;
  ConsumeKafkaTest() :
      logTestController_(LogTestController::getInstance()),
      logger_(logging::LoggerFactory<org::apache::nifi::minifi::processors::ConsumeKafka>::getLogger()) {
      reInitialize();
  }
  virtual ~ConsumeKafkaTest() {
    logTestController_.reset();
  }
  void reInitialize() {
    testController_.reset(new TestController());
    plan_ = testController_->createPlan();
    logTestController_.setDebug<TestPlan>();
    // logTestController_.setDebug<GenerateFlowFile>();
    // logTestController_.setDebug<UpdateAttribute>();
    logTestController_.setDebug<ConsumeKafka>();
    // logTestController_.setDebug<PutFile>();
    // logTestController_.setDebug<PutFile>();
    // logTestController_.setDebug<LogAttribute>();
    // logTestController_.setDebug<core::ProcessSession>();
    // logTestController_.setDebug<core::Connectable>();
    // logTestController_.setDebug<minifi::Connection>();
  }
  void singleConsumerWithPlainTextTest(
      bool expect_config_valid,
      const std::sting& expect_flowfiles_from_messages,  // to be upgraded
      bool expect_fixed_message_order,
      const std::vector<std::sting>& expect_header_attributes,
      const std::vector<std::sting>& messages_on_topic,
      const std::string& transactions_committed,  // to be upgraded
      const std::string& kafka_brokers,
      const std::string& security_protocol,
      const std::string& topic_names,
      const optional<std::string>& topic_name_format,
      const optional<std::string>& honor_transactions,
      const optional<std::string>& group_id,
      const optional<std::string>& offset_reset,
      const optional<std::string>& key_attribute_encoding,
      const optional<std::string>& message_demarcator,
      const optional<std::string>& message_header_encoding,
      const optional<std::string>& headers_to_add_as_attributes,
      const optional<std::string>& max_poll_records,
      const optional<std::string>& max_uncommitted_time,
      const optional<std::string>& communication_timeout) {
    reInitialize();

    // Relationships
    const core::Relationship success {"success", "description"};
    const core::Relationship failure {"failure", "description"};

    // Produrer chain
    std::shared_ptr<core::Processor> get_file      = plan_->addProcessor("GetFile", "generate", {success}, false);
    // maybe update_attribute for kafka key
    std::shared_ptr<core::Processor> publish_kafka = plan_->addProcessor("PublishKafka", "publish_kafka", {success, failure}, false);

    // Consumer chain
    std::shared_ptr<core::Processor> consume_kafka = plan_->addProcessor("ConsumeKafka", "publish_kafka", {success}, false);
    std::shared_ptr<core::Processor> extract_text  = plan_->addProcessor("ExtractText", "extract_text", {success}, false);
    std::shared_ptr<core::Processor> log_attribute = plan_->addProcessor("LogAttribute", "log_attribute", {success}, false);

    // Set up connections
    plan_->addConnection(get_file, success, publish_kafka);
    plan_->addConnection(consume_kafka, success, extract_text);

    // Auto-terminated relationships
    publish_kafka->setAutoTerminatedRelationships({success});
    publish_kafka->setAutoTerminatedRelationships({failure});
    log_attribute->setAutoTerminatedRelationships({success});

    plan_->setProperty(publish_kafka, PublishKafka::SeedBrokers.getName(), kafka_brokers);
    plan_->setProperty(publish_kafka, PublishKafka::Topic.getName(), PUBLISHER_CLIENT_NAME);  // FIXME(hunyadi): this only works when the topic name is an exact match
    plan_->setProperty(publish_kafka, PublishKafka::ClientName.getName(), kafka_brokers);

    plan_->setProperty(consume_kafka, ConsumeKafka::KafkaBrokers.getName(), kafka_brokers);
    plan_->setProperty(consume_kafka, ConsumeKafka::SecurityProtocol.getName(), security_protocol);
    plan_->setProperty(consume_kafka, ConsumeKafka::TopicNames.getName(), topic_names);
    plan_->setProperty(consume_kafka, ConsumeKafka::TopicNameFormat.getName(), topic_name_format);
    plan_->setProperty(consume_kafka, ConsumeKafka::HonorTransactions.getName(), honor_transactions);
    plan_->setProperty(consume_kafka, ConsumeKafka::GroupID.getName(), group_id);
    plan_->setProperty(consume_kafka, ConsumeKafka::OffsetReset.getName(), offset_reset);
    plan_->setProperty(consume_kafka, ConsumeKafka::KeyAttributeEncoding.getName(), key_attribute_encoding);
    plan_->setProperty(consume_kafka, ConsumeKafka::MessageDemarcator.getName(), message_demarcator);
    plan_->setProperty(consume_kafka, ConsumeKafka::MessageHeaderEncoding.getName(), message_header_encoding);
    plan_->setProperty(consume_kafka, ConsumeKafka::HeadersToAddAsAttributes.getName(), headers_to_add_as_attributes);
    plan_->setProperty(consume_kafka, ConsumeKafka::MaxPollRecords.getName(), max_poll_records);
    plan_->setProperty(consume_kafka, ConsumeKafka::MaxUncommittedTime.getName(), max_uncommitted_time);
    plan_->setProperty(consume_kafka, ConsumeKafka::CommunicationsTimeout.getName(), communication_timeout);

    plan_->setProperty(extract_text, ExtractText::Attribute.getName(), "true");

    plan_->setProperty(log_attribute, LogAttribute::LogPayload.getName(), "true");

    if (!expect_config_valid) {
      // TODO(hunyadi): Add function to the TestPlan that checks if scheduling ConsumeKafka succeeds
      const std::string input_dir = createTempDirWithFile(testController_.get(), TEST_FILE_NAME, message);
      plan_->setProperty(get_file, GetFile::Directory.getName(), input_dir);
      plan_->runNextProcessor();  // GetFile
      plan_->runNextProcessor();  // PublishKafka
      REQUIRE_THROWS(plan_->runNextProcessor());
      return;
    }

    std::vector<std::shared_ptr<core::FlowFile>> flowFilesProduced;
    for (const auto& message : messages_on_topic) {
      const std::string input_dir = createTempDirWithFile(testController_.get(), TEST_FILE_NAME, message);
      plan_->setProperty(get_file, GetFile::Directory.getName(), input_dir);
      plan_->runNextProcessor();  // GetFile
      plan_->runNextProcessor();  // PublishKafka
      plan_->runNextProcessor();  // ConsumeKafka
      if (!runCurrentProcessorUntilFlowfileIsProduced(MAX_CONSUMEKAFKA_POLL_TIME_SECONDS)) {
        FAIL("ConsumeKafka timed out when waiting to receive the message published to the kafka broker.");
      }
      plan_->runNextProcessor();  // ExtractText
      plan_->runNextProcessor();  // LogAttribute
      flowFilesProduced.emplace_back(plan_->getCurrentFlowFile());
    }

    const auto contentOrderOfFlowFile = [&] (const std::shared_ptr<core::FlowFile>& lhs, const std::shared_ptr<core::FlowFile>& rhs)
        { lhs.getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value() < rhs.getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value(); };
    REQUIRE_NO_THROW(std::sort(flowFilesProduced.begin(), flowFilesProduced.end(), contentOrderOfFlowFile));
    std::sort(messages_on_topic.begin(), messages_on_topic.end());
    const auto flowFileContentMatchesMessage = [&] (const std::shared_ptr<core::FlowFile>& flowfile, const std::string message) { lhs.getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT) == message; }
    ASSERT(std::equal(flowFilesProduced.begin(), flowFilesProduced.end(), messages_on_topic.begin(), messages_on_topic.end(), flowFileContentMatchesMessage));
  }

 private:
  bool runCurrentProcessorUntilFlowfileIsProduced(const std::crono::seconds& wait_duration) {
    isFlowFileProduced = [&] {
      if (plan_->getCurrentFlowFile()) {
        return true;
      }
      plan_->runCurrentProcessor();  // ConsumeKafka
      return false;
    };
    return verifyEventHappenedInPollTime(wait_duration, isFlowFileProduced);
  }

 private:
  static const std::crono::seconds MAX_CONSUMEKAFKA_POLL_TIME_SECONDS;
  static const std::string PUBLISHER_CLIENT_NAME;
  static const std::string ATTRIBUTE_FOR_CAPTURING_CONTENT;
  static const std::string TEST_FILE_NAME;

  std::unique_ptr<TestController> testController_;
  std::shared_ptr<TestPlan> plan_;
  LogTestController& logTestController_;
  std::shared_ptr<logging::Logger> logger_;
};

const std::string ConsumeKafkaTest::TEST_FILE_NAME{ "target_kafka_message.txt" };
const std::string ConsumeKafkaTest::PUBLISHER_CLIENT_NAME{ "consume_kafka_test_agent" };
const std::string ConsumeKafkaTest::ATTRIBUTE_FOR_CAPTURING_CONTENT{ "flowfile_content" };
const std::crono::seconds ConsumeKafkaTest::MAX_CONSUMEKAFKA_POLL_TIME_SECONDS{ 5 };

TEST_CASE_METHOD(ConsumeKafkaTest, "Publish and receive flow-files from Kafka.", "[ConsumeKafkaSingleConsumer]") {
  singleConsumerWithPlainTextTest(true, "both", false, {}, {"Ulysses", "James Joyce"}, "non-transactional messages", "localhost:9092", "PLAINTEXT", "ConsumeKafkaTest", {}, {}, {}, {}, {}, {}, {}, {}, {}, 1, {} )); // NOLINT
}

}  // namespace
