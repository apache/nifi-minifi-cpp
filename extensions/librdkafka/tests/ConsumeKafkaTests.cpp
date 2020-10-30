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
#include "../rdkafka_utils.h"
#include "../../standard-processors/processors/ExtractText.h"
#include "../../standard-processors/processors/LogAttribute.h"
#include "utils/file/FileUtils.h"
#include "utils/OptionalUtils.h"
#include "utils/RegexUtils.h"
#include "utils/StringUtils.h"
#include "utils/TestUtils.h"

#include "utils/IntegrationTestUtils.h"

namespace {
using org::apache::nifi::minifi::utils::optional;

class KafkaTestProducer {
 public:
  enum class PublishEvent {
    PUBLISH,
    TRANSACTION_START,
    TRANSACTION_COMMIT,
    CANCEL
  };
  KafkaTestProducer(const std::string& kafka_brokers, const std::string& topic, const bool transactional) :
      logger_(logging::LoggerFactory<KafkaTestProducer>::getLogger()) {
    using utils::setKafkaConfigurationField;

    std::unique_ptr<rd_kafka_conf_t, utils::rd_kafka_conf_deleter> conf = { rd_kafka_conf_new(), utils::rd_kafka_conf_deleter() };

    setKafkaConfigurationField(conf.get(), "bootstrap.servers", kafka_brokers);
    // setKafkaConfigurationField(conf.get(), "client.id", PRODUCER_CLIENT_NAME);
    setKafkaConfigurationField(conf.get(), "compression.codec", "snappy");
    setKafkaConfigurationField(conf.get(), "batch.num.messages", "1");  // FIXME(hunyadi): this has a default value, maybe not optional(?)

    if (transactional) {
      setKafkaConfigurationField(conf.get(), "transactional.id", "ConsumeKafkaTest_transaction_id");
    }

    static std::array<char, 512U> errstr{};
    producer_ = { rd_kafka_new(RD_KAFKA_PRODUCER, conf.release(), errstr.data(), errstr.size()), utils::rd_kafka_producer_deleter() };
    if (producer_ == nullptr) {
      auto error_msg = utils::StringUtils::join_pack("Failed to create Kafka producer %s", errstr.data());
      throw std::runtime_error(error_msg);
    }

    // The last argument is a config here, but it is already owned by the consumer. I assume that this would mean an override on the original config if used
    topic_ = { rd_kafka_topic_new(producer_.get(), topic.c_str(), nullptr), utils::rd_kafka_topic_deleter() };

    if (transactional) {
      rd_kafka_init_transactions(producer_.get(), TRANSACTIONS_TIMEOUT_MS.count());
    }
  }

  // Uses all the headers for every published message
  void publish_messages_to_topic(
      const std::vector<std::string>& messages_on_topic, const std::string& message_key, std::vector<PublishEvent> events,
      const std::vector<std::pair<std::string, std::string>>& message_headers, const optional<std::string>& message_header_encoding) {
    auto next_message = messages_on_topic.cbegin();
    for (const PublishEvent event : events) {
      switch (event) {
        case PublishEvent::PUBLISH:
          REQUIRE(messages_on_topic.cend() != next_message);
          publish_message(*next_message, message_key, message_headers, message_header_encoding);
          std::advance(next_message, 1);
          break;
        case PublishEvent::TRANSACTION_START:
          std::cerr << "Starting new transaction..." << std::endl;
          rd_kafka_begin_transaction(producer_.get());
          break;
        case PublishEvent::TRANSACTION_COMMIT:
          std::cerr << "Committing transaction..." << std::endl;
          rd_kafka_commit_transaction(producer_.get(), TRANSACTIONS_TIMEOUT_MS.count());
          break;
        case PublishEvent::CANCEL:
          std::cerr << "Cancelling transaction..." << std::endl;
          rd_kafka_abort_transaction(producer_.get(), TRANSACTIONS_TIMEOUT_MS.count());
      }
    }
  }

 private:
  void publish_message(const std::string& message, const std::string& message_key, const std::vector<std::pair<std::string, std::string>>& message_headers, const optional<std::string>& message_header_encoding) {
    logger_->log_debug("Producing: %s", message.c_str());
    std::unique_ptr<rd_kafka_headers_t, utils::rd_kafka_headers_deleter> headers(rd_kafka_headers_new(message_headers.size()), utils::rd_kafka_headers_deleter());
    if (!headers) {
      throw std::runtime_error("Generating message headers failed.");
    }
    for (const std::pair<std::string, std::string>& message_header : message_headers) {
      rd_kafka_header_add(headers.get(),
          const_cast<char*>(message_header.first.c_str()), message_header.first.size(),
          const_cast<char*>(message_header.second.c_str()), message_header.second.size());
    }

    if (RD_KAFKA_RESP_ERR_NO_ERROR != rd_kafka_producev(
        producer_.get(),
        RD_KAFKA_V_RKT(topic_.get()),
        RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
        RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
        RD_KAFKA_V_VALUE(const_cast<char*>(&message[0]), message.size()),
        RD_KAFKA_V_HEADERS(headers.release()),
        RD_KAFKA_V_KEY(message_key.c_str(), message_key.size()),
        // RD_KAFKA_V_OPAQUE(callback_ptr.get()),
        RD_KAFKA_V_END)) {
      logger_->log_error("Producer failure: %d: %s", rd_kafka_last_error(), rd_kafka_err2str(rd_kafka_last_error()));
    }
  }

  static const std::chrono::milliseconds TRANSACTIONS_TIMEOUT_MS;

  std::unique_ptr<rd_kafka_t, utils::rd_kafka_producer_deleter> producer_;
  std::unique_ptr<rd_kafka_topic_t, utils::rd_kafka_topic_deleter> topic_;

  std::shared_ptr<logging::Logger> logger_;
};

const std::chrono::milliseconds KafkaTestProducer::TRANSACTIONS_TIMEOUT_MS{ 2000 };

class ConsumeKafkaTest {
 public:
  using Processor = org::apache::nifi::minifi::core::Processor;
  using ConsumeKafka = org::apache::nifi::minifi::processors::ConsumeKafka;
  using ExtractText = org::apache::nifi::minifi::processors::ExtractText;
  using LogAttribute = org::apache::nifi::minifi::processors::LogAttribute;
  ConsumeKafkaTest() :
      logTestController_(LogTestController::getInstance()),
      logger_(logging::LoggerFactory<ConsumeKafkaTest>::getLogger()) {
      reInitialize();
  }
  virtual ~ConsumeKafkaTest() {
    logTestController_.reset();
  }
  void singleConsumerWithPlainTextTest(
      bool expect_config_valid,
      const std::string& expect_flowfiles_from_messages,  // TODO(hunyadi): upgrade to enum
      bool expect_fixed_message_order,
      const std::vector<std::pair<std::string, std::string>>& expect_header_attributes,
      const std::vector<std::string>& messages_on_topic,
      const std::vector<KafkaTestProducer::PublishEvent>& transaction_events,
      const std::vector<std::pair<std::string, std::string>>& message_headers,
      const std::string& kafka_brokers,
      const std::string& security_protocol,
      const std::string& topic_names,
      const optional<std::string>& topic_name_format,
      const optional<bool>& honor_transactions,
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

    // Consumer chain
    std::shared_ptr<core::Processor> consume_kafka = plan_->addProcessor("ConsumeKafka", "consume_kafka", {success}, false);
    std::shared_ptr<core::Processor> extract_text  = plan_->addProcessor("ExtractText", "extract_text", {success}, false);
    std::shared_ptr<core::Processor> log_attribute = plan_->addProcessor("LogAttribute", "log_attribute", {success}, false);

    // Set up connections
    plan_->addConnection(consume_kafka, success, extract_text);
    plan_->addConnection(extract_text, success, log_attribute);

    // Auto-terminated relationships
    log_attribute->setAutoTerminatedRelationships({success});

    auto optional_set_property = [&] (const std::shared_ptr<core::Processor>& processor, const std::string& property_name, const optional<std::string>& opt_value) {
      if (opt_value) {
        plan_->setProperty(processor, property_name, opt_value.value());
      }
    };

    const auto bool_to_string = [] (const bool b) -> std::string { return b ? "true" : "false"; };

    optional_set_property(consume_kafka, ConsumeKafka::KafkaBrokers.getName(), kafka_brokers);
    optional_set_property(consume_kafka, ConsumeKafka::SecurityProtocol.getName(), security_protocol);
    optional_set_property(consume_kafka, ConsumeKafka::TopicNames.getName(), topic_names);
    optional_set_property(consume_kafka, ConsumeKafka::TopicNameFormat.getName(), topic_name_format);
    optional_set_property(consume_kafka, ConsumeKafka::HonorTransactions.getName(), honor_transactions | utils::map(bool_to_string));
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
    plan_->schedule_processors();


    std::unique_ptr<rd_kafka_conf_t, utils::rd_kafka_conf_deleter> conf_;
    std::unique_ptr<rd_kafka_t, utils::rd_kafka_consumer_deleter> consumer_;

    const bool is_transactional = std::count(transaction_events.cbegin(), transaction_events.cend(), KafkaTestProducer::PublishEvent::TRANSACTION_START);
    const bool transactions_committed = transaction_events.back() == KafkaTestProducer::PublishEvent::TRANSACTION_COMMIT;

    KafkaTestProducer producer(kafka_brokers, PRODUCER_TOPIC, is_transactional);
    producer.publish_messages_to_topic(messages_on_topic, "key", transaction_events, message_headers, message_header_encoding);

    if (!expect_config_valid) {
      // TODO(hunyadi): Add function to the TestPlan that checks if scheduling ConsumeKafka succeeds
      const auto& message = messages_on_topic.front();
      REQUIRE_THROWS(plan_->runNextProcessor());
      return;
    }

    std::vector<std::shared_ptr<core::FlowFile>> flowFilesProduced;
    for (const auto& message : messages_on_topic) {
      plan_->increment_location();
      if ((honor_transactions && false == honor_transactions.value()) || (is_transactional && !transactions_committed)) {
        INFO("Non-committed messages received.");
        REQUIRE(false == plan_->runCurrentProcessorUntilFlowfileIsProduced(MAX_CONSUMEKAFKA_POLL_TIME_SECONDS));
        return;
      }
      {
        SCOPED_INFO("ConsumeKafka timed out when waiting to receive the message published to the kafka broker.");
        REQUIRE(plan_->runCurrentProcessorUntilFlowfileIsProduced(MAX_CONSUMEKAFKA_POLL_TIME_SECONDS));
      }
      plan_->runNextProcessor();  // ExtractText
      plan_->runNextProcessor();  // LogAttribute
      const std::shared_ptr<core::FlowFile> flow_file_produced = plan_->getFlowFileProducedByCurrentProcessor();
      for (const auto& exp_header : expect_header_attributes) {
        SCOPED_INFO("ConsumeKafka did not produce the expected flowfile attribute from message header: " << exp_header.first);
        const std::string header_attr = flow_file_produced->getAttribute(exp_header.first).value().get();
        REQUIRE(exp_header.second == header_attr);
      }
      {
        SCOPED_INFO("Message key is missing or incorrect (potential encoding mismatch).");
        REQUIRE("key" == decode_key(flow_file_produced->getAttribute(ConsumeKafka::KAFKA_MESSAGE_KEY_ATTR).value().get(), key_attribute_encoding));
      }
      flowFilesProduced.emplace_back(std::move(flow_file_produced));
      plan_->reset_location();
    }

    const auto contentOrderOfFlowFile = [&] (const std::shared_ptr<core::FlowFile>& lhs, const std::shared_ptr<core::FlowFile>& rhs) {
      // TODO(hunyadi): remove this extraction when done with debugging
      const auto l = lhs->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get();
      const auto r = rhs->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get();
      return lhs->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get() < rhs->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get();
    };
    {
      SCOPED_INFO("The flowfiles generated by ConsumeKafka are invalid (probably nullptr).");
      CHECK_NOTHROW(std::sort(flowFilesProduced.begin(), flowFilesProduced.end(), contentOrderOfFlowFile));
    }
    std::vector<std::string> sorted_messages(messages_on_topic);
    std::sort(sorted_messages.begin(), sorted_messages.end());
    const auto flowFileContentMatchesMessage = [&] (const std::shared_ptr<core::FlowFile>& flowfile, const std::string message) {
      return flowfile->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get() == message;
    };

    logger_->log_debug("************");
    std::string expected = "Expected: ";
    for (int i = 0; i < sorted_messages.size(); ++i) {
      expected += sorted_messages[i] + ", ";
      // logger_->log_debug("\u001b[33mExpected: %s\u001b[0m", sorted_messages[i].c_str());
    }
    std::string   actual = "  Actual: ";
    for (int i = 0; i < sorted_messages.size(); ++i) {
      actual += flowFilesProduced[i]->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get() + ", ";
      // logger_->log_debug("\u001b[33mActual: %s\u001b[0m", flowFilesProduced[0]->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value().get().c_str());
    }
    logger_->log_debug("\u001b[33m%s\u001b[0m", expected.c_str());
    logger_->log_debug("\u001b[33m%s\u001b[0m", actual.c_str());
    logger_->log_debug("************");

    INFO("The messages received by ConsumeKafka do not match those published");
    REQUIRE(std::equal(flowFilesProduced.begin(), flowFilesProduced.end(), sorted_messages.begin(), flowFileContentMatchesMessage));
  }

 private:
  void reInitialize() {
    testController_.reset(new TestController());
    plan_ = testController_->createPlan();
    logTestController_.setError<LogTestController>();
    logTestController_.setError<TestPlan>();
    logTestController_.setTrace<ConsumeKafka>();
    logTestController_.setTrace<ConsumeKafkaTest>();
    logTestController_.setTrace<KafkaTestProducer>();
    logTestController_.setDebug<ExtractText>();
    logTestController_.setError<LogAttribute>();
    // logTestController_.setDebug<core::ProcessSession>();
    logTestController_.setDebug<core::ProcessContext>();
    // logTestController_.setDebug<core::Connectable>();
    // logTestController_.setDebug<minifi::Connection>();
  }
  std::string decode_key(const std::string& key, const optional<std::string>& key_attribute_encoding) {
    if (!key_attribute_encoding || utils::StringUtils::equalsIgnoreCase(ConsumeKafka::KEY_ATTR_ENCODING_UTF_8, key_attribute_encoding.value())) {
      return key;
    }
    if (utils::StringUtils::equalsIgnoreCase(ConsumeKafka::ConsumeKafka::KEY_ATTR_ENCODING_HEX, key_attribute_encoding.value())) {
      return utils::StringUtils::from_hex(key);
    }
    throw std::runtime_error("Message Header Encoding does not match any of the presets in the test.");
  }

  static const std::chrono::seconds MAX_CONSUMEKAFKA_POLL_TIME_SECONDS;
  // static const std::string PRODUCER_CLIENT_NAME;
  static const std::string PRODUCER_TOPIC;
  static const std::string ATTRIBUTE_FOR_CAPTURING_CONTENT;
  static const std::string TEST_FILE_NAME_POSTFIX;

  std::unique_ptr<TestController> testController_;
  std::shared_ptr<TestPlan> plan_;
  LogTestController& logTestController_;
  std::shared_ptr<logging::Logger> logger_;
};

const std::string ConsumeKafkaTest::TEST_FILE_NAME_POSTFIX{ "target_kafka_message.txt" };
// const std::string ConsumeKafkaTest::PRODUCER_CLIENT_NAME{ "consume_kafka_test_agent" };
const std::string ConsumeKafkaTest::PRODUCER_TOPIC{ "ConsumeKafkaTest" };
const std::string ConsumeKafkaTest::ATTRIBUTE_FOR_CAPTURING_CONTENT{ "flowfile_content" };
const std::chrono::seconds ConsumeKafkaTest::MAX_CONSUMEKAFKA_POLL_TIME_SECONDS{ 5 };

TEST_CASE_METHOD(ConsumeKafkaTest, "Publish and receive flow-files from Kafka.", "[ConsumeKafkaSingleConsumer]") {
  const auto PUBLISH            = KafkaTestProducer::PublishEvent::PUBLISH;
  const auto TRANSACTION_START  = KafkaTestProducer::PublishEvent::TRANSACTION_START;
  const auto TRANSACTION_COMMIT = KafkaTestProducer::PublishEvent::TRANSACTION_COMMIT;
  const auto CANCEL             = KafkaTestProducer::PublishEvent::CANCEL;

  std::vector<KafkaTestProducer::PublishEvent> NON_TRANSACTIONAL_MESSAGES   { PUBLISH, PUBLISH };
  std::vector<KafkaTestProducer::PublishEvent> SINGLE_COMMITTED_TRANSACTION { TRANSACTION_START, PUBLISH, PUBLISH, TRANSACTION_COMMIT };
  std::vector<KafkaTestProducer::PublishEvent> TWO_SEPARATE_TRANSACTIONS    { TRANSACTION_START, PUBLISH, TRANSACTION_COMMIT, TRANSACTION_START, PUBLISH, TRANSACTION_COMMIT };
  std::vector<KafkaTestProducer::PublishEvent> NON_COMMITTED_TRANSACTION    { TRANSACTION_START, PUBLISH, PUBLISH };
  std::vector<KafkaTestProducer::PublishEvent> COMMIT_AND_CANCEL            { TRANSACTION_START, PUBLISH, CANCEL };

  //               EXP_CONF_VALID                                EXP_HEADER_ATTRIBUTES                                                                                                                                               KAFKA_BROKERS                                   TOPIC_NAME_FORMAT                             OFFSET_RESET                                           MESSAGE_HEADER_ENCODING                                               MAX_UNCOMMITTED_TIME // NOLINT
  //                      EXP_FF_FROM_MESSAGES                                                                                   MESSAGES_ON_TOPIC                                                                                            SECURITY_PROTOCOL                           HONOR_TRANSACTIONS                       KEY_ATTRIBUTE_ENCODING                                            HEADERS_TO_ADD_AS_ATTRIBUTES                                       // NOLINT
  //                           EXP_FIXED_MESSAGE_ORDER                                                                                                          TRANSACTION_TYPE                                                                                               TOPIC_NAMES                                      GROUP_ID                                     MESSAGE_DEMARCATOR                                                      MAX_POLL_RECORDS                      // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {              "Ulysses",                   "James Joyce"},   NON_TRANSACTIONAL_MESSAGES,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {     "The Great Gatsby",           "F. Scott Fitzgerald"},   NON_TRANSACTIONAL_MESSAGES,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",   "names",    {}, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  // singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {        "War and Peace",                   "Lev Tolstoy"},   NON_TRANSACTIONAL_MESSAGES,                                               {}, "localhost:9092", "PLAINTEXT", "a,b,c,ConsumeKafkaTest,d",   "names",    {}, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, { "Nineteen Eighty Four",                 "George Orwell"},   NON_TRANSACTIONAL_MESSAGES,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest", "pattern",    {}, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {               "Hamlet",           "William Shakespeare"},   NON_TRANSACTIONAL_MESSAGES,                                               {}, "localhost:9092", "PLAINTEXT",      "Cons[emu]*KafkaTest", "pattern",    {}, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT

  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {          "The Odyssey",                        "Ὅμηρος"},   NON_TRANSACTIONAL_MESSAGES,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {               "Lolita", "Владимир Владимирович Набоков"},   NON_TRANSACTIONAL_MESSAGES,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {}, {"utf-8"}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, { "Crime and Punishment",  "Фёдор Михайлович Достоевский"},   NON_TRANSACTIONAL_MESSAGES,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},   {"hex"}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {        "Paradise Lost",                   "John Milton"},   NON_TRANSACTIONAL_MESSAGES,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},   {"hEX"}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT

  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {  "Pride and Prejudice",                   "Jane Austen"}, SINGLE_COMMITTED_TRANSACTION,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {                 "Dune",                 "Frank Herbert"},    TWO_SEPARATE_TRANSACTIONS,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {      "The Black Sheep",              "Honore De Balzac"},    NON_COMMITTED_TRANSACTION,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {     "Gospel of Thomas",                                },            COMMIT_AND_CANCEL,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, { "Operation Dark Heart",                                },            COMMIT_AND_CANCEL,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {               "Brexit",                                },            COMMIT_AND_CANCEL,                                               {}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {}, false, {"test_group_id"}, {},        {}, {}, {},        {}, {}, "1 sec", "2 sec" ); // NOLINT

  singleConsumerWithPlainTextTest(true, "both", false,                 {{{"Rating"}, {"10/10"}}}, {             "Magician",              "Raymond E. Feist"},   NON_TRANSACTIONAL_MESSAGES,                        {{{"Rating"}, {"10/10"}}}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},        {}, {}, {}, {"Rating"}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,                                        {}, {             "Homeland",               "R. A. Salvatore"},   NON_TRANSACTIONAL_MESSAGES,             {{{"Contains dark elves"}, {"Yes"}}}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},        {}, {}, {},         {}, {}, "1 sec", "2 sec" ); // NOLINT
  // It is what it is, we seem to keep the first header when multiple ones of the same name are present, we cannot control which one to keep
  singleConsumerWithPlainTextTest(true, "both", false,                 {{{"Metal"}, {"Copper"}}}, {             "Mistborn",             "Brandon Sanderson"},   NON_TRANSACTIONAL_MESSAGES, {{{"Metal"}, {"Copper"}}, {{"Metal"}, {"Iron"}}}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},        {}, {}, {},  {"Metal"}, {}, "1 sec", "2 sec" ); // NOLINT
  singleConsumerWithPlainTextTest(true, "both", false,   {{{"Parts"}, {"First, second, third"}}}, {"The Lord of the Rings",              "J. R. R. Tolkien"},   NON_TRANSACTIONAL_MESSAGES,          {{{"Parts"}, {"First, second, third"}}}, "localhost:9092", "PLAINTEXT",         "ConsumeKafkaTest",        {},    {}, {"test_group_id"}, {},        {}, {}, {},  {"Parts"}, {}, "1 sec", "2 sec" ); // NOLINT
}

}  // namespace
