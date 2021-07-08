
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

#include "TestBase.h"

#include "../ConsumeKafka.h"
#include "../rdkafka_utils.h"
#include "../../standard-processors/processors/ExtractText.h"
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
    BEGIN_TRANSACTION,
    END_TRANSACTION,
    CANCEL_TRANSACTION
  };
  KafkaTestProducer(const std::string& kafka_brokers, const std::string& topic, const bool transactional) :
      logger_(logging::LoggerFactory<KafkaTestProducer>::getLogger()) {
    using utils::setKafkaConfigurationField;

    std::unique_ptr<rd_kafka_conf_t, utils::rd_kafka_conf_deleter> conf = { rd_kafka_conf_new(), utils::rd_kafka_conf_deleter() };

    setKafkaConfigurationField(*conf, "bootstrap.servers", kafka_brokers);
    setKafkaConfigurationField(*conf, "compression.codec", "snappy");
    setKafkaConfigurationField(*conf, "batch.num.messages", "1");

    if (transactional) {
      setKafkaConfigurationField(*conf, "transactional.id", "ConsumeKafkaTest_transaction_id");
    }

    static std::array<char, 512U> errstr{};
    producer_ = { rd_kafka_new(RD_KAFKA_PRODUCER, conf.release(), errstr.data(), errstr.size()), utils::rd_kafka_producer_deleter() };
    if (producer_ == nullptr) {
      auto error_msg = "Failed to create Kafka producer" + std::string{ errstr.data() };
      throw std::runtime_error(error_msg);
    }

    // The last argument is a config here, but it is already owned by the producer. I assume that this would mean an override on the original config if used
    topic_ = { rd_kafka_topic_new(producer_.get(), topic.c_str(), nullptr), utils::rd_kafka_topic_deleter() };

    if (transactional) {
      rd_kafka_init_transactions(producer_.get(), TRANSACTIONS_TIMEOUT_MS.count());
    }
  }

  // Uses all the headers for every published message
  void publish_messages_to_topic(
      const std::vector<std::string>& messages_on_topic, const std::string& message_key, std::vector<PublishEvent> events,
      const std::vector<std::pair<std::string, std::string>>& message_headers) {
    auto next_message = messages_on_topic.cbegin();
    for (const PublishEvent event : events) {
      switch (event) {
        case PublishEvent::PUBLISH:
          REQUIRE(messages_on_topic.cend() != next_message);
          publish_message(*next_message, message_key, message_headers);
          std::advance(next_message, 1);
          break;
        case PublishEvent::BEGIN_TRANSACTION:
          logger_->log_debug("Starting new transaction...");
          rd_kafka_begin_transaction(producer_.get());
          break;
        case PublishEvent::END_TRANSACTION:
          logger_->log_debug("Committing transaction...");
          rd_kafka_commit_transaction(producer_.get(), TRANSACTIONS_TIMEOUT_MS.count());
          break;
        case PublishEvent::CANCEL_TRANSACTION:
          logger_->log_debug("Cancelling transaction...");
          rd_kafka_abort_transaction(producer_.get(), TRANSACTIONS_TIMEOUT_MS.count());
      }
    }
  }

 private:
  void publish_message(
      const std::string& message, const std::string& message_key, const std::vector<std::pair<std::string, std::string>>& message_headers) {
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

  const KafkaTestProducer::PublishEvent PUBLISH            = KafkaTestProducer::PublishEvent::PUBLISH;
  const KafkaTestProducer::PublishEvent BEGIN_TRANSACTION  = KafkaTestProducer::PublishEvent::BEGIN_TRANSACTION;
  const KafkaTestProducer::PublishEvent END_TRANSACTION    = KafkaTestProducer::PublishEvent::END_TRANSACTION;
  const KafkaTestProducer::PublishEvent CANCEL_TRANSACTION = KafkaTestProducer::PublishEvent::CANCEL_TRANSACTION;

  const std::vector<KafkaTestProducer::PublishEvent> NON_TRANSACTIONAL_MESSAGES   { PUBLISH, PUBLISH };
  const std::vector<KafkaTestProducer::PublishEvent> SINGLE_COMMITTED_TRANSACTION { BEGIN_TRANSACTION, PUBLISH, PUBLISH, END_TRANSACTION };
  const std::vector<KafkaTestProducer::PublishEvent> TWO_SEPARATE_TRANSACTIONS    { BEGIN_TRANSACTION, PUBLISH, END_TRANSACTION, BEGIN_TRANSACTION, PUBLISH, END_TRANSACTION };
  const std::vector<KafkaTestProducer::PublishEvent> NON_COMMITTED_TRANSACTION    { BEGIN_TRANSACTION, PUBLISH, PUBLISH };
  const std::vector<KafkaTestProducer::PublishEvent> CANCELLED_TRANSACTION        { BEGIN_TRANSACTION, PUBLISH, CANCEL_TRANSACTION };

  const std::string KEEP_FIRST            = ConsumeKafka::MSG_HEADER_KEEP_FIRST;
  const std::string KEEP_LATEST           = ConsumeKafka::MSG_HEADER_KEEP_LATEST;
  const std::string COMMA_SEPARATED_MERGE = ConsumeKafka::MSG_HEADER_COMMA_SEPARATED_MERGE;

  static const std::string PRODUCER_TOPIC;
  static const std::string TEST_MESSAGE_KEY;

  // Relationships
  const core::Relationship success {"success", "description"};
  const core::Relationship failure {"failure", "description"};

  ConsumeKafkaTest() :
      logTestController_(LogTestController::getInstance()),
      logger_(logging::LoggerFactory<ConsumeKafkaTest>::getLogger()) {
      reInitialize();
  }

  virtual ~ConsumeKafkaTest() {
    logTestController_.reset();
  }

 protected:
  void reInitialize() {
    testController_.reset(new TestController());
    plan_ = testController_->createPlan();
    logTestController_.setError<LogTestController>();
    logTestController_.setError<TestPlan>();
    logTestController_.setTrace<ConsumeKafka>();
    logTestController_.setTrace<ConsumeKafkaTest>();
    logTestController_.setTrace<KafkaTestProducer>();
    logTestController_.setDebug<ExtractText>();
    logTestController_.setDebug<core::ProcessContext>();
  }

  void optional_set_property(const std::shared_ptr<core::Processor>& processor, const std::string& property_name, const optional<std::string>& opt_value) {
    if (opt_value) {
      plan_->setProperty(processor, property_name, opt_value.value());
    }
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

  std::vector<std::string> sort_and_split_messages(const std::vector<std::string>& messages_on_topic, const optional<std::string>& message_demarcator) {
    if (message_demarcator) {
      std::vector<std::string> sorted_split_messages;
      for (const auto& message : messages_on_topic) {
        std::vector<std::string> split_message = utils::StringUtils::split(message, message_demarcator.value());
        std::move(split_message.begin(), split_message.end(), std::back_inserter(sorted_split_messages));
      }
      std::sort(sorted_split_messages.begin(), sorted_split_messages.end());
      return sorted_split_messages;
    }
    std::vector<std::string> sorted_messages{ messages_on_topic.cbegin(), messages_on_topic.cend() };
    std::sort(sorted_messages.begin(), sorted_messages.end());
    return sorted_messages;
  }

  static const std::chrono::seconds MAX_CONSUMEKAFKA_POLL_TIME_SECONDS;
  static const std::string ATTRIBUTE_FOR_CAPTURING_CONTENT;
  static const std::string TEST_FILE_NAME_POSTFIX;

  std::unique_ptr<TestController> testController_;
  std::shared_ptr<TestPlan> plan_;
  LogTestController& logTestController_;
  std::shared_ptr<logging::Logger> logger_;
};

class ConsumeKafkaPropertiesTest : public ConsumeKafkaTest {
 public:
  ConsumeKafkaPropertiesTest() : ConsumeKafkaTest() {}
  virtual ~ConsumeKafkaPropertiesTest() {
    logTestController_.reset();
  }

  void single_consumer_with_plain_text_test(
      bool expect_config_valid,
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
      const optional<std::string>& duplicate_header_handling,
      const optional<std::string>& max_poll_records,
      const optional<std::string>& max_poll_time,
      const optional<std::string>& session_timeout) {
    reInitialize();

    // Consumer chain
    std::shared_ptr<core::Processor> consume_kafka = plan_->addProcessor("ConsumeKafka", "consume_kafka", {success}, false);
    std::shared_ptr<core::Processor> extract_text  = plan_->addProcessor("ExtractText", "extract_text", {success}, false);

    // Set up connections
    plan_->addConnection(consume_kafka, success, extract_text);
    extract_text->setAutoTerminatedRelationships({success});

    const auto bool_to_string = [] (const bool b) -> std::string { return b ? "true" : "false"; };

    plan_->setProperty(consume_kafka, ConsumeKafka::KafkaBrokers.getName(), kafka_brokers);
    plan_->setProperty(consume_kafka, ConsumeKafka::SecurityProtocol.getName(), security_protocol);
    plan_->setProperty(consume_kafka, ConsumeKafka::TopicNames.getName(), topic_names);

    optional_set_property(consume_kafka, ConsumeKafka::TopicNameFormat.getName(), topic_name_format);
    optional_set_property(consume_kafka, ConsumeKafka::HonorTransactions.getName(), honor_transactions | utils::map(bool_to_string));
    optional_set_property(consume_kafka, ConsumeKafka::GroupID.getName(), group_id);
    optional_set_property(consume_kafka, ConsumeKafka::OffsetReset.getName(), offset_reset);
    optional_set_property(consume_kafka, ConsumeKafka::KeyAttributeEncoding.getName(), key_attribute_encoding);
    optional_set_property(consume_kafka, ConsumeKafka::MessageDemarcator.getName(), message_demarcator);
    optional_set_property(consume_kafka, ConsumeKafka::MessageHeaderEncoding.getName(), message_header_encoding);
    optional_set_property(consume_kafka, ConsumeKafka::HeadersToAddAsAttributes.getName(), headers_to_add_as_attributes);
    optional_set_property(consume_kafka, ConsumeKafka::DuplicateHeaderHandling.getName(), duplicate_header_handling);
    optional_set_property(consume_kafka, ConsumeKafka::MaxPollRecords.getName(), max_poll_records);
    optional_set_property(consume_kafka, ConsumeKafka::MaxPollTime.getName(), max_poll_time);
    optional_set_property(consume_kafka, ConsumeKafka::SessionTimeout.getName(), session_timeout);

    plan_->setProperty(extract_text, ExtractText::Attribute.getName(), ATTRIBUTE_FOR_CAPTURING_CONTENT);

    if (!expect_config_valid) {
      REQUIRE_THROWS(plan_->scheduleProcessor(consume_kafka));
      return;
    } else {
      plan_->scheduleProcessors();
    }

    std::unique_ptr<rd_kafka_conf_t, utils::rd_kafka_conf_deleter> conf_;
    std::unique_ptr<rd_kafka_t, utils::rd_kafka_consumer_deleter> consumer_;

    const bool is_transactional = std::count(transaction_events.cbegin(), transaction_events.cend(), KafkaTestProducer::PublishEvent::BEGIN_TRANSACTION) != 0;
    const bool transactions_committed = transaction_events.back() == KafkaTestProducer::PublishEvent::END_TRANSACTION;

    KafkaTestProducer producer(kafka_brokers, PRODUCER_TOPIC, is_transactional);
    producer.publish_messages_to_topic(messages_on_topic, TEST_MESSAGE_KEY, transaction_events, message_headers);


    std::vector<std::shared_ptr<core::FlowFile>> flow_files_produced;
    for (std::size_t num_expected_messages_processed = 0; num_expected_messages_processed < messages_on_topic.size(); num_expected_messages_processed += std::stoi(max_poll_records.value_or("1"))) {
      plan_->increment_location();
      if ((honor_transactions && false == honor_transactions.value()) || (is_transactional && !transactions_committed)) {
        INFO("Non-committed messages received.");
        REQUIRE(false == plan_->runCurrentProcessorUntilFlowfileIsProduced(MAX_CONSUMEKAFKA_POLL_TIME_SECONDS));
        return;
      }
      {
        INFO("ConsumeKafka timed out when waiting to receive the message published to the kafka broker.");
        REQUIRE(plan_->runCurrentProcessorUntilFlowfileIsProduced(MAX_CONSUMEKAFKA_POLL_TIME_SECONDS));
      }
      std::size_t num_flow_files_produced = plan_->getNumFlowFileProducedByCurrentProcessor();
      plan_->increment_location();
      for (std::size_t times_extract_text_run = 0; times_extract_text_run < num_flow_files_produced; ++times_extract_text_run) {
        plan_->runCurrentProcessor();  // ExtractText
        std::shared_ptr<core::FlowFile> flow_file = plan_->getFlowFileProducedByCurrentProcessor();
        for (const auto& exp_header : expect_header_attributes) {
          INFO("ConsumeKafka did not produce the expected flowfile attribute from message header: " << exp_header.first << ".");
          const auto header_attr_opt = flow_file->getAttribute(exp_header.first);
          REQUIRE(header_attr_opt);
          REQUIRE(exp_header.second == header_attr_opt.value());
        }
        {
          INFO("Message key is missing or incorrect (potential encoding mismatch).");
          REQUIRE(TEST_MESSAGE_KEY == decode_key(flow_file->getAttribute(ConsumeKafka::KAFKA_MESSAGE_KEY_ATTR).value(), key_attribute_encoding));
          REQUIRE("1" == flow_file->getAttribute(ConsumeKafka::KAFKA_COUNT_ATTR).value());
          REQUIRE(flow_file->getAttribute(ConsumeKafka::KAFKA_OFFSET_ATTR));
          REQUIRE(flow_file->getAttribute(ConsumeKafka::KAFKA_PARTITION_ATTR));
          REQUIRE(PRODUCER_TOPIC == flow_file->getAttribute(ConsumeKafka::KAFKA_TOPIC_ATTR).value());
        }
        flow_files_produced.emplace_back(std::move(flow_file));
      }
      plan_->reset_location();
    }

    const auto contentOrderOfFlowFile = [&] (const std::shared_ptr<core::FlowFile>& lhs, const std::shared_ptr<core::FlowFile>& rhs) {
      return lhs->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value() < rhs->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value();
    };
    {
      INFO("The flowfiles generated by ConsumeKafka are invalid (probably nullptr).");
      REQUIRE_NOTHROW(std::sort(flow_files_produced.begin(), flow_files_produced.end(), contentOrderOfFlowFile));
    }
    std::vector<std::string> sorted_split_messages = sort_and_split_messages(messages_on_topic, message_demarcator);
    const auto flow_file_content_matches_message = [&] (const std::shared_ptr<core::FlowFile>& flowfile, const std::string message) {
      return flowfile->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value() == message;
    };

    logger_->log_debug("************");
    std::string expected = "Expected: ";
    for (std::size_t i = 0; i < sorted_split_messages.size(); ++i) {
      expected += sorted_split_messages[i] + ", ";
    }
    std::string   actual = "  Actual: ";
    for (std::size_t i = 0; i < sorted_split_messages.size(); ++i) {
      actual += flow_files_produced[i]->getAttribute(ATTRIBUTE_FOR_CAPTURING_CONTENT).value() + ", ";
    }
    logger_->log_debug("%s", expected.c_str());
    logger_->log_debug("%s", actual.c_str());
    logger_->log_debug("************");

    INFO("The messages received by ConsumeKafka do not match those published");
    REQUIRE(std::equal(flow_files_produced.begin(), flow_files_produced.end(), sorted_split_messages.begin(), flow_file_content_matches_message));
  }
};

class ConsumeKafkaContinuousPublishingTest : public ConsumeKafkaTest {
 public:
  ConsumeKafkaContinuousPublishingTest() : ConsumeKafkaTest() {}
  virtual ~ConsumeKafkaContinuousPublishingTest() {
    logTestController_.reset();
  }

  void single_consumer_with_continuous_message_producing(
      const uint64_t msg_periodicity_ms,
      const std::string& kafka_brokers,
      const optional<std::string>& group_id,
      const optional<std::string>& max_poll_records,
      const optional<std::string>& max_poll_time,
      const optional<std::string>& session_timeout) {
    reInitialize();

    std::shared_ptr<core::Processor> consume_kafka = plan_->addProcessor("ConsumeKafka", "consume_kafka", {success}, false);

    plan_->setProperty(consume_kafka, "allow.auto.create.topics", "true", true);  // Seems like the topic tests work without this

    plan_->setProperty(consume_kafka, ConsumeKafka::KafkaBrokers.getName(), kafka_brokers);
    plan_->setProperty(consume_kafka, ConsumeKafka::TopicNames.getName(), PRODUCER_TOPIC);
    optional_set_property(consume_kafka, ConsumeKafka::GroupID.getName(), group_id);

    optional_set_property(consume_kafka, ConsumeKafka::MaxPollRecords.getName(), max_poll_records);
    optional_set_property(consume_kafka, ConsumeKafka::MaxPollTime.getName(), max_poll_time);
    optional_set_property(consume_kafka, ConsumeKafka::SessionTimeout.getName(), session_timeout);

    consume_kafka->setAutoTerminatedRelationships({success});

    KafkaTestProducer producer("localhost:9092", PRODUCER_TOPIC, /* transactional = */ false);

    std::atomic_bool producer_loop_stop{ false };
    const auto producer_loop = [&] {
      std::size_t num_messages_sent = 0;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      while (!producer_loop_stop) {
        producer.publish_messages_to_topic({ "Message after " + std::to_string(msg_periodicity_ms * ++num_messages_sent) + " ms"}, TEST_MESSAGE_KEY, { PUBLISH }, {});
        std::this_thread::sleep_for(std::chrono::milliseconds(msg_periodicity_ms));
      }
    };

    plan_->scheduleProcessors();

    const auto get_time_property_ms = [] (const std::string& property_string) {
      int64_t value{};
      org::apache::nifi::minifi::core::TimeUnit unit{};
      REQUIRE(org::apache::nifi::minifi::core::Property::StringToTime(property_string, value, unit));
      int64_t value_as_ms = 0;
      REQUIRE(org::apache::nifi::minifi::core::Property::ConvertTimeUnitToMS(value, unit, value_as_ms));
      return value_as_ms;
    };

    std::thread producer_thread(producer_loop);
    CHECK_NOTHROW(plan_->runNextProcessor());
    producer_loop_stop = true;
    producer_thread.join();

    std::size_t num_flow_files_produced = plan_->getNumFlowFileProducedByCurrentProcessor();

    const uint64_t max_poll_time_ms = get_time_property_ms(max_poll_time.value_or(ConsumeKafka::DEFAULT_MAX_POLL_TIME));
    const uint64_t max_poll_records_value = max_poll_records ? std::stoi(max_poll_records.value()) : ConsumeKafka::DEFAULT_MAX_POLL_RECORDS;
    const uint64_t exp_lower_bound = std::min(max_poll_time_ms / msg_periodicity_ms - 2, max_poll_records_value);
    const uint64_t exp_upper_bound = std::min(max_poll_time_ms / msg_periodicity_ms + 2, max_poll_records_value);
    logger_->log_debug("Max poll time: %d, Max poll records: %d, Exp. flowfiles produced: (min: %d, max: %d), actual: %d",
        max_poll_time_ms, max_poll_records_value, exp_lower_bound, exp_upper_bound, num_flow_files_produced);

    REQUIRE(exp_lower_bound <= num_flow_files_produced);
    REQUIRE(num_flow_files_produced <= exp_upper_bound);
  }
};

const std::string ConsumeKafkaTest::TEST_FILE_NAME_POSTFIX{ "target_kafka_message.txt" };
const std::string ConsumeKafkaTest::TEST_MESSAGE_KEY{ "consume_kafka_test_key" };
const std::string ConsumeKafkaTest::PRODUCER_TOPIC{ "ConsumeKafkaTest" };
const std::string ConsumeKafkaTest::ATTRIBUTE_FOR_CAPTURING_CONTENT{ "flowfile_content" };
const std::chrono::seconds ConsumeKafkaTest::MAX_CONSUMEKAFKA_POLL_TIME_SECONDS{ 5 };

TEST_CASE_METHOD(ConsumeKafkaPropertiesTest, "ConsumeKafka parses and uses kafka topics.", "[ConsumeKafka][Kafka][Topic]") {
  auto run_tests = [&] (const std::vector<std::string>& messages_on_topic, const std::string& topic_names, const optional<std::string>& topic_name_format) {
    single_consumer_with_plain_text_test(true, {}, messages_on_topic, NON_TRANSACTIONAL_MESSAGES, {}, "localhost:9092", "PLAINTEXT", topic_names, topic_name_format, {}, "test_group_id", {}, {}, {}, {}, {}, {}, "1", "2 sec", "60 sec"); // NOLINT
  };
  run_tests({ "Ulysses",              "James Joyce"         }, "ConsumeKafkaTest",         {});
  run_tests({ "The Great Gatsby",     "F. Scott Fitzgerald" }, "ConsumeKafkaTest",         ConsumeKafka::TOPIC_FORMAT_NAMES);
  run_tests({ "War and Peace",        "Lev Tolstoy"         }, "a,b,c,ConsumeKafkaTest,d", ConsumeKafka::TOPIC_FORMAT_NAMES);
  run_tests({ "Nineteen Eighty Four", "George Orwell"       }, "ConsumeKafkaTest",         ConsumeKafka::TOPIC_FORMAT_PATTERNS);
  run_tests({ "Hamlet",               "William Shakespeare" }, "Cons[emu]*KafkaTest",      ConsumeKafka::TOPIC_FORMAT_PATTERNS);
}

TEST_CASE_METHOD(ConsumeKafkaPropertiesTest, "Offsets are reset to the latest when a consumer starts with non-processed messages.", "[ConsumeKafka][Kafka][OffsetReset]") {
  auto run_tests = [&] (
      const std::vector<std::string>& messages_on_topic,
      const std::vector<KafkaTestProducer::PublishEvent>& transaction_events) {
    single_consumer_with_plain_text_test(true, {}, messages_on_topic, transaction_events, {}, "localhost:9092", "PLAINTEXT", "ConsumeKafkaTest", {}, {}, "test_group_id", {}, {}, {}, {}, {}, {}, "1", "2 sec", "60 sec"); // NOLINT
  };
  KafkaTestProducer producer("localhost:9092", PRODUCER_TOPIC, false);
  producer.publish_messages_to_topic({"Dummy messages", "that should be ignored", "due to offset reset on ConsumeKafka startup"}, TEST_MESSAGE_KEY, {PUBLISH, PUBLISH, PUBLISH}, {});
  run_tests({"Brave New World",  "Aldous Huxley"}, NON_TRANSACTIONAL_MESSAGES);
  producer.publish_messages_to_topic({"Dummy messages", "that should be ignored", "due to offset reset on ConsumeKafka startup"}, TEST_MESSAGE_KEY, {PUBLISH, PUBLISH, PUBLISH}, {});
  run_tests({"Call of the Wild", "Jack London"}, NON_TRANSACTIONAL_MESSAGES);
}

TEST_CASE_METHOD(ConsumeKafkaPropertiesTest, "Key attribute is encoded according to the \"Key Attribute Encoding\" property.", "[ConsumeKafka][Kafka][KeyAttributeEncoding]") {
  auto run_tests = [&] (const std::vector<std::string>& messages_on_topic, const optional<std::string>& key_attribute_encoding) {
    single_consumer_with_plain_text_test(true, {}, messages_on_topic, NON_TRANSACTIONAL_MESSAGES, {}, "localhost:9092", "PLAINTEXT", "ConsumeKafkaTest", {}, {}, "test_group_id", {}, key_attribute_encoding, {}, {}, {}, {}, "1", "2 sec", "60 sec"); // NOLINT
  };

  run_tests({ "The Odyssey",          "Ὅμηρος"                        }, {});
  run_tests({ "Lolita",               "Владимир Владимирович Набоков" }, "utf-8");
  run_tests({ "Crime and Punishment", "Фёдор Михайлович Достоевский"  }, "hex");
  run_tests({ "Paradise Lost",        "John Milton"                   }, "hEX");
}

TEST_CASE_METHOD(ConsumeKafkaPropertiesTest, "Transactional behaviour is supported.", "[ConsumeKafka][Kafka][Transaction]") {
  auto run_tests = [&] (const std::vector<std::string>& messages_on_topic, const std::vector<KafkaTestProducer::PublishEvent>& transaction_events, const optional<bool>& honor_transactions) {
    single_consumer_with_plain_text_test(true, {}, messages_on_topic, transaction_events, {}, "localhost:9092", "PLAINTEXT", "ConsumeKafkaTest", {}, honor_transactions, "test_group_id", {}, {}, {}, {}, {}, {}, "1", "2 sec", "60 sec"); // NOLINT
  };
  run_tests({  "Pride and Prejudice", "Jane Austen"      }, SINGLE_COMMITTED_TRANSACTION, {});
  run_tests({                 "Dune", "Frank Herbert"    },    TWO_SEPARATE_TRANSACTIONS, {});
  run_tests({      "The Black Sheep", "Honore De Balzac" },    NON_COMMITTED_TRANSACTION, {});
  run_tests({     "Gospel of Thomas"                     },        CANCELLED_TRANSACTION, {});
  run_tests({ "Operation Dark Heart"                     },        CANCELLED_TRANSACTION, true);
  run_tests({               "Brexit"                     },        CANCELLED_TRANSACTION, false);
}

TEST_CASE_METHOD(ConsumeKafkaPropertiesTest, "Headers on consumed Kafka messages are extracted into attributes if requested on ConsumeKafka.", "[ConsumeKafka][Kafka][Headers]") {
  auto run_tests = [&] (
      const std::vector<std::string>& messages_on_topic,
      const std::vector<std::pair<std::string, std::string>>& expect_header_attributes,
      const std::vector<std::pair<std::string, std::string>>& message_headers,
      const optional<std::string>& headers_to_add_as_attributes,
      const optional<std::string>& duplicate_header_handling) {
    single_consumer_with_plain_text_test(true, expect_header_attributes, messages_on_topic, NON_TRANSACTIONAL_MESSAGES, message_headers, "localhost:9092", "PLAINTEXT", "ConsumeKafkaTest", {}, {}, "test_group_id", {}, {}, {}, {}, headers_to_add_as_attributes, duplicate_header_handling, "1", "2 sec", "60 sec"); // NOLINT
  };
  run_tests({             "Homeland",   "R. A. Salvatore"},                                      {},             {{{"Contains dark elves"}, {"Yes"}}},         {},                    {});
  run_tests({             "Magician",  "Raymond E. Feist"},               {{{"Rating"}, {"10/10"}}},                        {{{"Rating"}, {"10/10"}}}, {"Rating"},                    {});
  run_tests({             "Mistborn", "Brandon Sanderson"},               {{{"Metal"}, {"Copper"}}}, {{{"Metal"}, {"Copper"}}, {{"Metal"}, {"Iron"}}},  {"Metal"},            KEEP_FIRST);
  run_tests({             "Mistborn", "Brandon Sanderson"},                 {{{"Metal"}, {"Iron"}}}, {{{"Metal"}, {"Copper"}}, {{"Metal"}, {"Iron"}}},  {"Metal"},           KEEP_LATEST);
  run_tests({             "Mistborn", "Brandon Sanderson"},         {{{"Metal"}, {"Copper, Iron"}}}, {{{"Metal"}, {"Copper"}}, {{"Metal"}, {"Iron"}}},  {"Metal"}, COMMA_SEPARATED_MERGE);
  run_tests({"The Lord of the Rings",  "J. R. R. Tolkien"}, {{{"Parts"}, {"First, second, third"}}},          {{{"Parts"}, {"First, second, third"}}},  {"Parts"},                    {});
}

TEST_CASE_METHOD(ConsumeKafkaPropertiesTest, "Messages are separated into multiple flowfiles if the message demarcator is present in the message.", "[ConsumeKafka][Kafka][MessageDemarcator]") {
  auto run_tests = [&] (
      const std::vector<std::string>& messages_on_topic,
      const optional<std::string>& message_demarcator) {
    single_consumer_with_plain_text_test(true, {}, messages_on_topic, NON_TRANSACTIONAL_MESSAGES, {}, "localhost:9092", "PLAINTEXT", "ConsumeKafkaTest", {}, {}, "test_group_id", {}, {}, message_demarcator, {}, {}, {}, "1", "2 sec", "60 sec"); // NOLINT
  };
  run_tests({"Barbapapa", "Anette Tison and Talus Taylor"}, "a");
}

TEST_CASE_METHOD(ConsumeKafkaPropertiesTest, "The maximum poll records allows ConsumeKafka to combine multiple messages into a single flowfile.", "[ConsumeKafka][Kafka][Batching][MaxPollRecords]") {
  auto run_tests = [&] (
      const std::vector<std::string>& messages_on_topic,
      const std::vector<KafkaTestProducer::PublishEvent>& transaction_events,
      const optional<std::string>& max_poll_records) {
    single_consumer_with_plain_text_test(true, {}, messages_on_topic, transaction_events, {}, "localhost:9092", "PLAINTEXT", "ConsumeKafkaTest", {}, {}, "test_group_id", {}, {}, {}, {}, {}, {}, max_poll_records, "2 sec", "60 sec"); // NOLINT
  };
  run_tests({"The Count of Monte Cristo", "Alexandre Dumas"}, NON_TRANSACTIONAL_MESSAGES, "2");

  const std::vector<std::string> content {
      "Make const member functions thread safe",
      "Understand special member function generation",
      "Use std::unique_ptr for exclusive-ownership resource management",
      "Use std::shared_ptr for shared-ownership resource management",
      "Use std::weak_ptr for std::shared_ptr-like pointers that can dangle",
      "Prefer std::make_unique and std::make_shared to direct use of new",
      "When using the Pimpl Idiom, define special member functions inthe implementation file",
      "Understand std::move and std::forward",
      "Distinguish universal references from rvalue references",
      "Use std::move on rvalue references, std::forward on universal references",
      "Avoid overloading on universal references",
      "Familiarize yourself with alternatives to overloading on universal references",
      "Understand reference collapsing",
      "Assume that move operations are not present, not cheap, and not used",
      "Familiarize yourself with perfect forwarding failure cases",
      "Avoid default capture modes",
      "Use init capture to move objects into closures",
      "Use decltype on auto&& parameters to std::forward them",
      "Prefer lambdas to std::bind",
      "Prefer task-based programming to thread-based" };
  const std::vector<KafkaTestProducer::PublishEvent> transaction_events(content.size(), PUBLISH);
  run_tests(content, transaction_events, "5");
}

TEST_CASE_METHOD(ConsumeKafkaPropertiesTest, "Non-plain text security context throws scheduling exceptions.", "[ConsumeKafka][Kafka][SecurityProtocol]") {
  single_consumer_with_plain_text_test(false, {}, { "Miyamoto Musashi", "Eiji Yoshikawa" }, NON_TRANSACTIONAL_MESSAGES, {}, "localhost:9092", ConsumeKafka::SECURITY_PROTOCOL_SSL, "ConsumeKafkaTest", {}, {}, "test_group_id", {}, {}, {}, {}, {}, {}, "1", "2 sec", "60 sec"); // NOLINT
}

TEST_CASE_METHOD(ConsumeKafkaPropertiesTest, "Acceptable values for message header and key attribute encoding are \"UTF-8\" and \"hex\".", "[ConsumeKafka][Kafka][InvalidEncoding]") {
  single_consumer_with_plain_text_test(false, {}, {                           "Shogun", "James Clavell" }, NON_TRANSACTIONAL_MESSAGES, {}, "localhost:9092", "PLAINTEXT", "ConsumeKafkaTest", {}, {}, "test_group_id", {}, "UTF-32", {},       {}, {}, {}, "1", "2 sec", "60 sec"); // NOLINT
  single_consumer_with_plain_text_test(false, {}, { "Alice's Adventures in Wonderland", "Lewis Carroll" }, NON_TRANSACTIONAL_MESSAGES, {}, "localhost:9092", "PLAINTEXT", "ConsumeKafkaTest", {}, {}, "test_group_id", {},       {}, {}, "UTF-32", {}, {}, "1", "2 sec", "60 sec"); // NOLINT
}

TEST_CASE_METHOD(ConsumeKafkaContinuousPublishingTest, "ConsumeKafka can spend no more time polling than allowed in the maximum poll time property.", "[ConsumeKafka][Kafka][Batching][MaxPollTime]") {
  auto run_tests = [&] (
      const uint64_t msg_periodicity_ms,
      const optional<std::string>& max_poll_records,
      const optional<std::string>& max_poll_time,
      const optional<std::string>& session_timeout) {
    single_consumer_with_continuous_message_producing(msg_periodicity_ms, "localhost:9092", "test_group_id", max_poll_records, max_poll_time, session_timeout);
  };
  // For some reason, a session time-out of a few seconds does not work at all, 10 seconds seems to be stable
  run_tests(300, "20", "3 seconds", "10000 ms");
  // Running multiple tests does not work properly here. For some reason, producing messages
  // while a rebalance is triggered causes this error, and a blocked poll when new
  // messages are produced:
  //     Group "test_group_id" heartbeat error response in state up (join state wait-revoke-rebalance_cb, 1 partition(s) assigned): Broker: Group rebalance in progress
  //
  //  I tried adding a wait time for more than "session.timeout.ms" inbetween tests, but it was not sufficient
}

}  // namespace
