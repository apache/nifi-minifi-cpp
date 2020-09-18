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

#include <algorithm>
#include <memory>
#include <string>
#include <set>

// #include "TestBase.h"
#include "../../../libminifi/test/TestBase.h"

#include "../ConsumeKafka.h"
#include "../PublishKafka.h"
#include "../../standard-processors/processors/ExtractText.h"
#include "../../standard-processors/processors/GetFile.h"
#include "../../standard-processors/processors/LogAttribute.h"
#include "utils/file/FileUtils.h"
#include "utils/OptionalUtils.h"
#include "utils/RegexUtils.h"
#include "utils/TestUtils.h"
#include "utils/IntegrationTestUtils.h"

namespace {
using org::apache::nifi::minifi::utils::optional;
using org::apache::nifi::minifi::utils::createTempDir;
using org::apache::nifi::minifi::utils::putFileToDir;

class ConsumeKafkaTest {
 public:
  using Processor = org::apache::nifi::minifi::core::Processor;
  using PublishKafka = org::apache::nifi::minifi::processors::PublishKafka;
  using ConsumeKafka = org::apache::nifi::minifi::processors::ConsumeKafka;
  using GetFile = org::apache::nifi::minifi::processors::GetFile;
  using ExtractText = org::apache::nifi::minifi::processors::ExtractText;
  using LogAttribute = org::apache::nifi::minifi::processors::LogAttribute;
  ConsumeKafkaTest() :
      logTestController_(LogTestController::getInstance()),
      logger_(logging::LoggerFactory<org::apache::nifi::minifi::processors::ConsumeKafka>::getLogger()) {
      reInitialize();
  }
  virtual ~ConsumeKafkaTest() {
    logTestController_.reset();
  }
  void singleConsumerWithPlainTextTest(
      bool expect_config_valid,
      const std::string& expect_flowfiles_from_messages,  // to be upgraded
      bool expect_fixed_message_order,
      const std::vector<std::string>& expect_header_attributes,
      const std::vector<std::string>& messages_on_topic,
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

    // logger_->log_debug("Testing with the following args:");
    // // logger_->log_debug("expect_config_valid: %s", expect_config_valid ? "true" : "false");
    // // logger_->log_debug("expect_flowfiles_from_messages: %s", expect_flowfiles_from_messages.c_str());
    // // logger_->log_debug("expect_fixed_message_order: %s", expect_fixed_message_order ? "true" : "false");
    // // logger_->log_debug("expect_header_attributes: %s", expect_header_attributes.size() ? expect_header_attributes.back().c_str() : "");  // FIXME(hunyad)
    // // logger_->log_debug("messages_on_topic: %s", messages_on_topic.size() ? messages_on_topic.back().c_str() : "");  // FIXME(hunyadi)
    // // logger_->log_debug("transactions_committed: %s", transactions_committed.c_str());
    // // logger_->log_debug("kafka_brokers: %s", kafka_brokers.c_str());
    // // logger_->log_debug("security_protocol: %s", security_protocol.c_str());
    // logger_->log_debug("topic_names: %s", topic_names.c_str());
    // const std::string topic_name_format_value = topic_name_format.value_or("");
    // logger_->log_debug("topic_name_format: %s", topic_name_format_value.c_str());
    // const std::string honor_transactions_value = honor_transactions.value_or("");
    // logger_->log_debug("honor_transactions: %s", honor_transactions_value.c_str());
    // const std::string group_id_value = group_id.value_or("");
    // logger_->log_debug("group_id: %s", group_id_value.c_str());
    // const std::string offset_reset_value = offset_reset.value_or("");
    // logger_->log_debug("offset_reset: %s", offset_reset_value.c_str());
    // logger_->log_debug("key_attribute_encoding: %s", key_attribute_encoding.value_or("").c_str());
    // logger_->log_debug("message_demarcator: %s", message_demarcator.value_or("").c_str());
    // logger_->log_debug("message_header_encoding: %s", message_header_encoding.value_or("").c_str());
    // logger_->log_debug("headers_to_add_as_attributes: %s", headers_to_add_as_attributes.value_or("").c_str());
    // logger_->log_debug("max_poll_records: %s", max_poll_records.value_or("").c_str());
    // logger_->log_debug("max_uncommitted_time: %s", max_uncommitted_time.value_or("").c_str());
    // logger_->log_debug("communication_timeout: %s", communication_timeout.value_or("").c_str());

    // Relationships
    const core::Relationship success {"success", "description"};
    const core::Relationship failure {"failure", "description"};

    // Produrer chain
    std::shared_ptr<core::Processor> get_file      = plan_->addProcessor("GetFile", "generate", {success}, false);
    // maybe update_attribute for kafka key
    std::shared_ptr<core::Processor> publish_kafka = plan_->addProcessor("PublishKafka", "publish_kafka", {success, failure}, false);

    // Consumer chain
    std::shared_ptr<core::Processor> consume_kafka = plan_->addProcessor("ConsumeKafka", "consume_kafka", {success}, false);
    logger_->log_debug("%d consume_kafka.use_count(): %d", __LINE__, consume_kafka.use_count());

    std::shared_ptr<core::Processor> extract_text  = plan_->addProcessor("ExtractText", "extract_text", {success}, false);
    std::shared_ptr<core::Processor> log_attribute = plan_->addProcessor("LogAttribute", "log_attribute", {success}, false);

    logger_->log_debug("%d consume_kafka.use_count(): %d", __LINE__, consume_kafka.use_count());

    // Set up connections
    plan_->addConnection(get_file, success, publish_kafka);
    logger_->log_debug("%d consume_kafka.use_count(): %d", __LINE__, consume_kafka.use_count());
    plan_->addConnection(consume_kafka, success, extract_text);
    logger_->log_debug("%d consume_kafka.use_count(): %d", __LINE__, consume_kafka.use_count());
    plan_->addConnection(extract_text, success, log_attribute);

    // Auto-terminated relationships
    publish_kafka->setAutoTerminatedRelationships({success, failure});
    log_attribute->setAutoTerminatedRelationships({success});

    const std::string input_dir = createTempDir(testController_.get());
    plan_->setProperty(get_file, GetFile::Directory.getName(), input_dir);

    plan_->setProperty(publish_kafka, PublishKafka::SeedBrokers.getName(), kafka_brokers);
    plan_->setProperty(publish_kafka, PublishKafka::Topic.getName(), PUBLISHER_TOPIC);  // FIXME(hunyadi): this only works when the topic name is an exact match
    plan_->setProperty(publish_kafka, PublishKafka::QueueBufferMaxTime.getName(), "50 msec");
    plan_->setProperty(publish_kafka, PublishKafka::ClientName.getName(), PUBLISHER_CLIENT_NAME);

    auto optional_set_property = [&] (const std::shared_ptr<core::Processor>& processor, const std::string& property_name, const optional<std::string>& opt_value) {
      if (opt_value) {
        plan_->setProperty(processor, property_name, opt_value.value());
      }
    };

    optional_set_property(consume_kafka, ConsumeKafka::KafkaBrokers.getName(), kafka_brokers);
    optional_set_property(consume_kafka, ConsumeKafka::SecurityProtocol.getName(), security_protocol);
    optional_set_property(consume_kafka, ConsumeKafka::TopicNames.getName(), topic_names);
    optional_set_property(consume_kafka, ConsumeKafka::TopicNameFormat.getName(), topic_name_format);
    optional_set_property(consume_kafka, ConsumeKafka::HonorTransactions.getName(), honor_transactions);
    optional_set_property(consume_kafka, ConsumeKafka::GroupID.getName(), group_id);
    optional_set_property(consume_kafka, ConsumeKafka::OffsetReset.getName(), offset_reset);
    optional_set_property(consume_kafka, ConsumeKafka::KeyAttributeEncoding.getName(), key_attribute_encoding);
    optional_set_property(consume_kafka, ConsumeKafka::MessageDemarcator.getName(), message_demarcator);
    optional_set_property(consume_kafka, ConsumeKafka::MessageHeaderEncoding.getName(), message_header_encoding);
    optional_set_property(consume_kafka, ConsumeKafka::HeadersToAddAsAttributes.getName(), headers_to_add_as_attributes);
    optional_set_property(consume_kafka, ConsumeKafka::MaxPollRecords.getName(), max_poll_records);
    optional_set_property(consume_kafka, ConsumeKafka::MaxUncommittedTime.getName(), max_uncommitted_time);
    optional_set_property(consume_kafka, ConsumeKafka::CommunicationsTimeout.getName(), communication_timeout);

    plan_->setProperty(extract_text, ExtractText::Attribute.getName(), ATTRIBUTE_FOR_CAPTURING_CONTENT);

    plan_->setProperty(log_attribute, LogAttribute::LogPayload.getName(), "true");
    logger_->log_debug("%d consume_kafka.use_count(): %d", __LINE__, consume_kafka.use_count());

    plan_->schedule_processors();
    logger_->log_debug("%d consume_kafka.use_count(): %d", __LINE__, consume_kafka.use_count());

    if (!expect_config_valid) {
      // TODO(hunyadi): Add function to the TestPlan that checks if scheduling ConsumeKafka succeeds
      const auto& message = messages_on_topic.front();
      putFileToDir(input_dir, message + TEST_FILE_NAME_POSTFIX, message);
      plan_->runNextProcessor();  // GetFile
      plan_->runNextProcessor();  // PublishKafka
      // FIXME(hunyadi): replace this with catch2 REQUIRE_THROWS
      // REQUIRE_THROWS(plan_->runNextProcessor());
      try {
        plan_->runNextProcessor();
        assert(false);
      }
      catch(...) {}
      return;
    }
    logger_->log_debug("%d consume_kafka.use_count(): %d", __LINE__, consume_kafka.use_count());


    std::vector<std::shared_ptr<core::FlowFile>> flowFilesProduced;
    for (const auto& message : messages_on_topic) {
      putFileToDir(input_dir, message + TEST_FILE_NAME_POSTFIX, message);
      plan_->runNextProcessor();  // GetFile
      plan_->runNextProcessor();  // PublishKafka
      plan_->increment_location();
      if (!plan_->runCurrentProcessorUntilFlowfileIsProduced(MAX_CONSUMEKAFKA_POLL_TIME_SECONDS)) {
        FAIL("ConsumeKafka timed out when waiting to receive the message published to the kafka broker.");
      }
      plan_->runNextProcessor();  // ExtractText
      plan_->runNextProcessor();  // LogAttribute
      flowFilesProduced.emplace_back(plan_->getFlowFileProducedByCurrentProcessor());
      plan_->reset_location();
    }
    logger_->log_debug("%d consume_kafka.use_count(): %d", __LINE__, consume_kafka.use_count());


    const auto contentOrderOfFlowFile = [&] (const std::shared_ptr<core::FlowFile>& lhs, const std::shared_ptr<core::FlowFile>& rhs) {
      // TODO(hunyadi): remove this extraction when done with debugging
      const auto l = lhs->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get();
      const auto r = rhs->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get();
      return lhs->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get() < rhs->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get();
    };
    // FIXME(hunyadi): replace this with catch2 REQUIRE_NO_THROW
    // REQUIRE_NO_THROW(std::sort(flowFilesProduced.begin(), flowFilesProduced.end(), contentOrderOfFlowFile));
    try {
      std::sort(flowFilesProduced.begin(), flowFilesProduced.end(), contentOrderOfFlowFile);
    }
    catch(...) { assert(false); }
    std::vector<std::string> sorted_messages(messages_on_topic);
    std::sort(sorted_messages.begin(), sorted_messages.end());
    const auto flowFileContentMatchesMessage = [&] (const std::shared_ptr<core::FlowFile>& flowfile, const std::string message) {
      return flowfile->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get() == message;
    };
    logger_->log_debug("%d consume_kafka.use_count(): %d", __LINE__, consume_kafka.use_count());


    logger_->log_debug("************");
    std::string expected = "Expected: ";
    std::string   actual = "  Actual: ";
    for (int i = 0; i < sorted_messages.size(); ++i) {
      expected += sorted_messages[i] + ", ";
      // logger_->log_debug("\u001b[33mExpected: %s\u001b[0m", sorted_messages[i].c_str());
    }
    for (int i = 0; i < sorted_messages.size(); ++i) {
      actual += flowFilesProduced[i]->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get() + ", ";
      // logger_->log_debug("\u001b[33mActual: %s\u001b[0m", flowFilesProduced[0]->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get().c_str());
    }
    logger_->log_debug("\u001b[33m%s\u001b[0m", expected.c_str());
    logger_->log_debug("\u001b[33m%s\u001b[0m", actual.c_str());
    logger_->log_debug("************");

    assert(std::equal(flowFilesProduced.begin(), flowFilesProduced.end(), sorted_messages.begin(), flowFileContentMatchesMessage));
  }

 private:
  void reInitialize() {
    testController_.reset(new TestController());
    plan_ = testController_->createPlan();
    logTestController_.setError<LogTestController>();
    logTestController_.setError<TestPlan>();
    // logTestController_.setDebug<GetFile>();
    logTestController_.setDebug<PublishKafka>();
    logTestController_.setTrace<ConsumeKafka>();
    logTestController_.setDebug<ExtractText>();
    logTestController_.setError<LogAttribute>();
    // logTestController_.setDebug<core::ProcessSession>();
    logTestController_.setDebug<core::ProcessContext>();
    // logTestController_.setDebug<core::Connectable>();
    // logTestController_.setDebug<minifi::Connection>();
  }

  static const std::chrono::seconds MAX_CONSUMEKAFKA_POLL_TIME_SECONDS;
  static const std::string PUBLISHER_CLIENT_NAME;
  static const std::string PUBLISHER_TOPIC;
  static const std::string ATTRIBUTE_FOR_CAPTURING_CONTENT;
  static const std::string TEST_FILE_NAME_POSTFIX;

  std::unique_ptr<TestController> testController_;
  std::shared_ptr<TestPlan> plan_;
  LogTestController& logTestController_;
  std::shared_ptr<logging::Logger> logger_;
};

const std::string ConsumeKafkaTest::TEST_FILE_NAME_POSTFIX{ "target_kafka_message.txt" };
const std::string ConsumeKafkaTest::PUBLISHER_CLIENT_NAME{ "consume_kafka_test_agent" };
const std::string ConsumeKafkaTest::PUBLISHER_TOPIC{ "ConsumeKafkaTest" };
const std::string ConsumeKafkaTest::ATTRIBUTE_FOR_CAPTURING_CONTENT{ "flowfile_content" };
const std::chrono::seconds ConsumeKafkaTest::MAX_CONSUMEKAFKA_POLL_TIME_SECONDS{ 7 };

TEST_CASE_METHOD(ConsumeKafkaTest, "Publish and receive flow-files from Kafka.", "[ConsumeKafkaSingleConsumer]") {
  singleConsumerWithPlainTextTest(true, "both", false, {}, {              "Ulysses",                   "James Joyce"},    "non-transactional messages", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, {     "The Great Gatsby",           "F. Scott Fitzgerald"},    "non-transactional messages", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest", { "names"   }, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, {        "War and Peace",                   "Lev Tolstoy"},    "non-transactional messages", "localhost:9092", "PLAINTEXT", "a,b,c,ConsumeKafkaTest,d", { "names"   }, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, { "Nineteen Eighty Four",                 "George Orwell"},    "non-transactional messages", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest", { "pattern" }, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, {               "Hamlet",           "William Shakespeare"},    "non-transactional messages", "localhost:9092", "PLAINTEXT",      "Cons[emu]*KafkaTest", { "pattern" }, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT

  // TODO(hunyadi): Encoded messages yet to be tested
  singleConsumerWithPlainTextTest(true, "both", false, {}, {          "The Odyssey",                        "Ὅμηρος"},    "non-transactional messages", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, {               "Lolita", "Владимир Владимирович Набоков"},    "non-transactional messages", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {}, {"utf-8"}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, { "Crime and Punishment",  "Фёдор Михайлович Достоевский"},    "non-transactional messages", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},   {"hex"}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT

  // TODO(hunyadi): Transactions are yet to be tested
  singleConsumerWithPlainTextTest(true, "both", false, {}, {  "Pride and Prejudice",                   "Jane Austen"}, "single, committed transaction", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, {                 "Dune",                 "Frank Herbert"},     "two separate transactions", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, {      "The Black Sheep",              "Honore De Balzac"},     "non-committed transaction", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, {     "Gospel of Thomas",                      "(cancel)"},             "commit and cancel", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, { "Operation Dark Heart",                      "(cancel)"},             "commit and cancel", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, {               "Brexit",                      "(cancel)"},             "commit and cancel", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT

  // TODO(hunyadi): Message headers are yet to be tested
  singleConsumerWithPlainTextTest(true, "both", false, {}, {             "Magician",              "Raymond E. Feist"},    "non-transactional messages", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, {             "Homeland",               "R. A. Salvatore"},    "non-transactional messages", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, {             "Mistborn",             "Brandon Sanderson"},    "non-transactional messages", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false, {}, {"The Lord of the Rings",              "J. R. R. Tolkien"},    "non-transactional messages", "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",            {}, {}, {"test_group_id"}, {},        {}, {}, {}, {}, {}, "1 sec", "4 sec" ); // NOLINT
}

}  // namespace
