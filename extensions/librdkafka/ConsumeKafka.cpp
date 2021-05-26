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

#include "ConsumeKafka.h"

#include <algorithm>
#include <limits>

#include "core/PropertyValidation.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
// The upper limit for Max Poll Time is 4 seconds. This is because Watchdog would potentially start
// reporting issues with the processor health otherwise
class ConsumeKafkaMaxPollTimeValidator : public TimePeriodValidator {
 public:
  ConsumeKafkaMaxPollTimeValidator(const std::string &name) // NOLINT
      : TimePeriodValidator(name) {
  }
  ~ConsumeKafkaMaxPollTimeValidator() override = default;

  ValidationResult validate(const std::string& subject, const std::string& input) const override {
    uint64_t value;
    TimeUnit timeUnit;
    uint64_t value_as_ms;
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(
        core::TimePeriodValue::StringToTime(input, value, timeUnit) &&
        org::apache::nifi::minifi::core::Property::ConvertTimeUnitToMS(value, timeUnit, value_as_ms) &&
        0 < value_as_ms && value_as_ms <= 4000).build();
  }
};
}  // namespace core
namespace processors {

constexpr const std::size_t ConsumeKafka::DEFAULT_MAX_POLL_RECORDS;
constexpr char const* ConsumeKafka::DEFAULT_MAX_POLL_TIME;

constexpr char const* ConsumeKafka::TOPIC_FORMAT_NAMES;
constexpr char const* ConsumeKafka::TOPIC_FORMAT_PATTERNS;

core::Property ConsumeKafka::KafkaBrokers(core::PropertyBuilder::createProperty("Kafka Brokers")
  ->withDescription("A comma-separated list of known Kafka Brokers in the format <host>:<port>.")
  ->withDefaultValue("localhost:9092", core::StandardValidators::get().NON_BLANK_VALIDATOR)
  ->supportsExpressionLanguage(true)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::SecurityProtocol(core::PropertyBuilder::createProperty("Security Protocol")
  ->withDescription("This property is currently not supported. Protocol used to communicate with brokers. Corresponds to Kafka's 'security.protocol' property.")
  ->withAllowableValues<std::string>({SECURITY_PROTOCOL_PLAINTEXT/*, SECURITY_PROTOCOL_SSL, SECURITY_PROTOCOL_SASL_PLAINTEXT, SECURITY_PROTOCOL_SASL_SSL*/ })
  ->withDefaultValue(SECURITY_PROTOCOL_PLAINTEXT)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::TopicNames(core::PropertyBuilder::createProperty("Topic Names")
  ->withDescription("The name of the Kafka Topic(s) to pull from. Multiple topic names are supported as a comma separated list.")
  ->supportsExpressionLanguage(true)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::TopicNameFormat(core::PropertyBuilder::createProperty("Topic Name Format")
  ->withDescription("Specifies whether the Topic(s) provided are a comma separated list of names or a single regular expression.")
  ->withAllowableValues<std::string>({TOPIC_FORMAT_NAMES, TOPIC_FORMAT_PATTERNS})
  ->withDefaultValue(TOPIC_FORMAT_NAMES)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::HonorTransactions(core::PropertyBuilder::createProperty("Honor Transactions")
  ->withDescription(
      "Specifies whether or not MiNiFi should honor transactional guarantees when communicating with Kafka. If false, the Processor will use an \"isolation level\" of "
      "read_uncomitted. This means that messages will be received as soon as they are written to Kafka but will be pulled, even if the producer cancels the transactions. "
      "If this value is true, MiNiFi will not receive any messages for which the producer's transaction was canceled, but this can result in some latency since the consumer "
      "must wait for the producer to finish its entire transaction instead of pulling as the messages become available.")
  ->withDefaultValue<bool>(true)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::GroupID(core::PropertyBuilder::createProperty("Group ID")
  ->withDescription("A Group ID is used to identify consumers that are within the same consumer group. Corresponds to Kafka's 'group.id' property.")
  ->supportsExpressionLanguage(true)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::OffsetReset(core::PropertyBuilder::createProperty("Offset Reset")
  ->withDescription("Allows you to manage the condition when there is no initial offset in Kafka or if the current offset does not exist any more on the server (e.g. because that "
      "data has been deleted). Corresponds to Kafka's 'auto.offset.reset' property.")
  ->withAllowableValues<std::string>({OFFSET_RESET_EARLIEST, OFFSET_RESET_LATEST, OFFSET_RESET_NONE})
  ->withDefaultValue(OFFSET_RESET_LATEST)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::KeyAttributeEncoding(core::PropertyBuilder::createProperty("Key Attribute Encoding")
  ->withDescription("FlowFiles that are emitted have an attribute named 'kafka.key'. This property dictates how the value of the attribute should be encoded.")
  ->withAllowableValues<std::string>({KEY_ATTR_ENCODING_UTF_8, KEY_ATTR_ENCODING_HEX})
  ->withDefaultValue(KEY_ATTR_ENCODING_UTF_8)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::MessageDemarcator(core::PropertyBuilder::createProperty("Message Demarcator")
  ->withDescription("Since KafkaConsumer receives messages in batches, you have an option to output FlowFiles which contains all Kafka messages in a single batch "
      "for a given topic and partition and this property allows you to provide a string (interpreted as UTF-8) to use for demarcating apart multiple Kafka messages. "
      "This is an optional property and if not provided each Kafka message received will result in a single FlowFile which time it is triggered. ")
  ->supportsExpressionLanguage(true)
  ->build());

core::Property ConsumeKafka::MessageHeaderEncoding(core::PropertyBuilder::createProperty("Message Header Encoding")
  ->withDescription("Any message header that is found on a Kafka message will be added to the outbound FlowFile as an attribute. This property indicates the Character Encoding "
      "to use for deserializing the headers.")
  ->withAllowableValues<std::string>({MSG_HEADER_ENCODING_UTF_8, MSG_HEADER_ENCODING_HEX})
  ->withDefaultValue(MSG_HEADER_ENCODING_UTF_8)
  ->build());

core::Property ConsumeKafka::HeadersToAddAsAttributes(core::PropertyBuilder::createProperty("Headers To Add As Attributes")
  ->withDescription("A comma separated list to match against all message headers. Any message header whose name matches an item from the list will be added to the FlowFile "
      "as an Attribute. If not specified, no Header values will be added as FlowFile attributes. The behaviour on when multiple headers of the same name are present is set using "
      "the DuplicateHeaderHandling attribute.")
  ->build());

core::Property ConsumeKafka::DuplicateHeaderHandling(core::PropertyBuilder::createProperty("Duplicate Header Handling")
  ->withDescription("For headers to be added as attributes, this option specifies how to handle cases where multiple headers are present with the same key. "
      "For example in case of receiving these two headers: \"Accept: text/html\" and \"Accept: application/xml\" and we want to attach the value of \"Accept\" "
      "as a FlowFile attribute:\n"
      " - \"Keep First\" attaches: \"Accept -> text/html\"\n"
      " - \"Keep Latest\" attaches: \"Accept -> application/xml\"\n"
      " - \"Comma-separated Merge\" attaches: \"Accept -> text/html, application/xml\"\n")
  ->withAllowableValues<std::string>({MSG_HEADER_KEEP_FIRST, MSG_HEADER_KEEP_LATEST, MSG_HEADER_COMMA_SEPARATED_MERGE})
  ->withDefaultValue(MSG_HEADER_KEEP_LATEST)  // Mirroring NiFi behaviour
  ->build());

core::Property ConsumeKafka::MaxPollRecords(core::PropertyBuilder::createProperty("Max Poll Records")
  ->withDescription("Specifies the maximum number of records Kafka should return when polling each time the processor is triggered.")
  ->withDefaultValue<unsigned int>(DEFAULT_MAX_POLL_RECORDS)
  ->build());

core::Property ConsumeKafka::MaxPollTime(core::PropertyBuilder::createProperty("Max Poll Time")
  ->withDescription("Specifies the maximum amount of time the consumer can use for polling data from the brokers. "
      "Polling is a blocking operation, so the upper limit of this value is specified in 4 seconds.")
  ->withDefaultValue(DEFAULT_MAX_POLL_TIME, std::make_shared<core::ConsumeKafkaMaxPollTimeValidator>(std::string("ConsumeKafkaMaxPollTimeValidator")))
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::SessionTimeout(core::PropertyBuilder::createProperty("Session Timeout")
  ->withDescription("Client group session and failure detection timeout. The consumer sends periodic heartbeats "
      "to indicate its liveness to the broker. If no hearts are received by the broker for a group member within "
      "the session timeout, the broker will remove the consumer from the group and trigger a rebalance. "
      "The allowed range is configured with the broker configuration properties group.min.session.timeout.ms and group.max.session.timeout.ms.")
  ->withDefaultValue<core::TimePeriodValue>("60 seconds")
  ->build());

const core::Relationship ConsumeKafka::Success("success", "Incoming kafka messages as flowfiles. Depending on the demarcation strategy, this can be one or multiple flowfiles per message.");

void ConsumeKafka::initialize() {
  setSupportedProperties({
    KafkaBrokers,
    SecurityProtocol,
    TopicNames,
    TopicNameFormat,
    HonorTransactions,
    GroupID,
    OffsetReset,
    KeyAttributeEncoding,
    MessageDemarcator,
    MessageHeaderEncoding,
    HeadersToAddAsAttributes,
    DuplicateHeaderHandling,
    MaxPollRecords,
    MaxPollTime,
    SessionTimeout
  });
  setSupportedRelationships({
    Success,
  });
}

void ConsumeKafka::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /* sessionFactory */) {
  gsl_Expects(context);
  // Required properties
  kafka_brokers_                = utils::getRequiredPropertyOrThrow(context, KafkaBrokers.getName());
  security_protocol_            = utils::getRequiredPropertyOrThrow(context, SecurityProtocol.getName());
  topic_names_                  = utils::listFromRequiredCommaSeparatedProperty(context, TopicNames.getName());
  topic_name_format_            = utils::getRequiredPropertyOrThrow(context, TopicNameFormat.getName());
  honor_transactions_           = utils::parseBooleanPropertyOrThrow(context, HonorTransactions.getName());
  group_id_                     = utils::getRequiredPropertyOrThrow(context, GroupID.getName());
  offset_reset_                 = utils::getRequiredPropertyOrThrow(context, OffsetReset.getName());
  key_attribute_encoding_       = utils::getRequiredPropertyOrThrow(context, KeyAttributeEncoding.getName());
  max_poll_time_milliseconds_   = utils::parseTimePropertyMSOrThrow(context, MaxPollTime.getName());
  session_timeout_milliseconds_ = utils::parseTimePropertyMSOrThrow(context, SessionTimeout.getName());

  // Optional properties
  context->getProperty(MessageDemarcator.getName(), message_demarcator_);
  context->getProperty(MessageHeaderEncoding.getName(), message_header_encoding_);
  context->getProperty(DuplicateHeaderHandling.getName(), duplicate_header_handling_);

  headers_to_add_as_attributes_ = utils::listFromCommaSeparatedProperty(context, HeadersToAddAsAttributes.getName());
  max_poll_records_ = gsl::narrow<std::size_t>(utils::getOptionalUintProperty(*context, MaxPollRecords.getName()).value_or(DEFAULT_MAX_POLL_RECORDS));

  // For now security protocols are not yet supported
  if (!utils::StringUtils::equalsIgnoreCase(SECURITY_PROTOCOL_PLAINTEXT, security_protocol_)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Security protocols are not supported yet.");
  }

  if (!utils::StringUtils::equalsIgnoreCase(KEY_ATTR_ENCODING_UTF_8, key_attribute_encoding_) && !utils::StringUtils::equalsIgnoreCase(KEY_ATTR_ENCODING_HEX, key_attribute_encoding_)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Unsupported key attribute encoding: " + key_attribute_encoding_);
  }

  if (!utils::StringUtils::equalsIgnoreCase(MSG_HEADER_ENCODING_UTF_8, message_header_encoding_) && !utils::StringUtils::equalsIgnoreCase(MSG_HEADER_ENCODING_HEX, message_header_encoding_)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Unsupported message header encoding: " + key_attribute_encoding_);
  }

  configure_new_connection(*context);
}

namespace {
void rebalance_cb(rd_kafka_t* rk, rd_kafka_resp_err_t trigger, rd_kafka_topic_partition_list_t* partitions, void* /*opaque*/) {
  // Cooperative, incremental assignment is not supported in the current librdkafka version
  std::shared_ptr<logging::Logger> logger{logging::LoggerFactory<ConsumeKafka>::getLogger()};
  logger->log_debug("Rebalance triggered.");
  rd_kafka_resp_err_t assign_error = RD_KAFKA_RESP_ERR_NO_ERROR;
  switch (trigger) {
    case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
      logger->log_debug("assigned:");
      if (logger->should_log(core::logging::LOG_LEVEL::debug)) {
        utils::print_topics_list(*logger, *partitions);
      }
      assign_error = rd_kafka_assign(rk, partitions);
      break;

    case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
      logger->log_debug("revoked:");
      rd_kafka_commit(rk, partitions, /* async = */ 0);  // Sync commit, maybe unneccessary
      if (logger->should_log(core::logging::LOG_LEVEL::debug)) {
        utils::print_topics_list(*logger, *partitions);
      }
      assign_error = rd_kafka_assign(rk, NULL);
      break;

    default:
      logger->log_debug("failed: %s", rd_kafka_err2str(trigger));
      assign_error = rd_kafka_assign(rk, NULL);
      break;
  }
  logger->log_debug("assign failure: %s", rd_kafka_err2str(assign_error));
}
}  // namespace

void ConsumeKafka::create_topic_partition_list() {
  kf_topic_partition_list_ = { rd_kafka_topic_partition_list_new(topic_names_.size()), utils::rd_kafka_topic_partition_list_deleter() };

  // On subscriptions any topics prefixed with ^ will be regex matched
  if (utils::StringUtils::equalsIgnoreCase(TOPIC_FORMAT_PATTERNS, topic_name_format_)) {
    for (const std::string& topic : topic_names_) {
      const std::string regex_format = "^" + topic;
      rd_kafka_topic_partition_list_add(kf_topic_partition_list_.get(), regex_format.c_str(), RD_KAFKA_PARTITION_UA);
    }
  } else {
    for (const std::string& topic : topic_names_) {
      rd_kafka_topic_partition_list_add(kf_topic_partition_list_.get(), topic.c_str(), RD_KAFKA_PARTITION_UA);
    }
  }

  // Subscribe to topic set using balanced consumer groups
  // Subscribing from the same process without an inbetween unsubscribe call
  // Does not seem to be triggering a rebalance (maybe librdkafka bug?)
  // This might happen until the cross-overship between processors and connections are settled
  rd_kafka_resp_err_t subscribe_response = rd_kafka_subscribe(consumer_.get(), kf_topic_partition_list_.get());
  if (RD_KAFKA_RESP_ERR_NO_ERROR != subscribe_response) {
    logger_->log_error("rd_kafka_subscribe error %d: %s", subscribe_response, rd_kafka_err2str(subscribe_response));
  }
}

void ConsumeKafka::extend_config_from_dynamic_properties(const core::ProcessContext& context) {
  using utils::setKafkaConfigurationField;

  const std::vector<std::string> dynamic_prop_keys = context.getDynamicPropertyKeys();
  if (dynamic_prop_keys.empty()) {
    return;
  }
  logger_->log_info("Loading %d extra kafka configuration fields from ConsumeKafka dynamic properties:", dynamic_prop_keys.size());
  for (const std::string& key : dynamic_prop_keys) {
    std::string value;
    gsl_Expects(context.getDynamicProperty(key, value));
    logger_->log_info("%s: %s", key.c_str(), value.c_str());
    setKafkaConfigurationField(*conf_, key, value);
  }
}

void ConsumeKafka::configure_new_connection(const core::ProcessContext& context) {
  using utils::setKafkaConfigurationField;

  conf_ = { rd_kafka_conf_new(), utils::rd_kafka_conf_deleter() };
  if (conf_ == nullptr) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to create rd_kafka_conf_t object");
  }

  // Set rebalance callback for use with coordinated consumer group balancing
  // Rebalance handlers are needed for the initial configuration of the consumer
  // If they are not set, offset reset is ignored and polling produces messages
  // Registering a rebalance_cb turns off librdkafka's automatic partition assignment/revocation and instead delegates that responsibility to the application's rebalance_cb.
  rd_kafka_conf_set_rebalance_cb(conf_.get(), rebalance_cb);

  // Uncomment this for librdkafka debug logs:
  // setKafkaConfigurationField(conf_.get(), "debug", "all");

  setKafkaConfigurationField(*conf_, "bootstrap.servers", kafka_brokers_);
  setKafkaConfigurationField(*conf_, "auto.offset.reset", "latest");
  setKafkaConfigurationField(*conf_, "enable.auto.commit", "false");
  setKafkaConfigurationField(*conf_, "enable.auto.offset.store", "false");
  setKafkaConfigurationField(*conf_, "isolation.level", honor_transactions_ ? "read_committed" : "read_uncommitted");
  setKafkaConfigurationField(*conf_, "group.id", group_id_);
  setKafkaConfigurationField(*conf_, "session.timeout.ms", std::to_string(session_timeout_milliseconds_.count()));
  setKafkaConfigurationField(*conf_, "max.poll.interval.ms", "600000");  // Twice the default, arbitrarily chosen

  // This is a librdkafka option, but the communication timeout is also specified in each of the
  // relevant API calls. Could be redundant, but it probably does not hurt to set this
  setKafkaConfigurationField(*conf_, "metadata.request.timeout.ms", std::to_string(METADATA_COMMUNICATIONS_TIMEOUT_MS));

  extend_config_from_dynamic_properties(context);

  std::array<char, 512U> errstr{};
  consumer_ = { rd_kafka_new(RD_KAFKA_CONSUMER, conf_.release(), errstr.data(), errstr.size()), utils::rd_kafka_consumer_deleter() };
  if (consumer_ == nullptr) {
    const std::string error_msg { errstr.data() };
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to create Kafka consumer " + error_msg);
  }

  create_topic_partition_list();

  // Changing the partition list should happen only as part as the initialization of offsets
  // a function like `rd_kafka_position()` might have unexpected effects
  // for instance when a consumer gets assigned a partition it used to
  // consume at an earlier rebalance.
  //
  // As far as I understand, instead of rd_kafka_position() an rd_kafka_committed() call if preferred here,
  // as it properly fetches offsets from the broker
  if (RD_KAFKA_RESP_ERR_NO_ERROR != rd_kafka_committed(consumer_.get(), kf_topic_partition_list_.get(), METADATA_COMMUNICATIONS_TIMEOUT_MS)) {
    logger_->log_error("Retrieving committed offsets for topics+partitions failed.");
  }

  rd_kafka_resp_err_t poll_set_consumer_response = rd_kafka_poll_set_consumer(consumer_.get());
  if (RD_KAFKA_RESP_ERR_NO_ERROR != poll_set_consumer_response) {
    logger_->log_error("rd_kafka_poll_set_consumer error %d: %s", poll_set_consumer_response, rd_kafka_err2str(poll_set_consumer_response));
  }

  // There is no rd_kafka_seek alternative for rd_kafka_topic_partition_list_t, only rd_kafka_topic_t
  // rd_kafka_topic_partition_list_set_offset should reset the offsets to the latest (or whatever is set in the config),
  // Also, rd_kafka_committed should also fetch and set latest the latest offset
  // In reality, neither of them seem to work (not even with calling rd_kafka_position())
  logger_->log_info("Resetting offset manually.");
  while (true) {
    std::unique_ptr<rd_kafka_message_t, utils::rd_kafka_message_deleter>
        message_wrapper{ rd_kafka_consumer_poll(consumer_.get(), max_poll_time_milliseconds_.count()), utils::rd_kafka_message_deleter() };

    if (!message_wrapper || RD_KAFKA_RESP_ERR_NO_ERROR != message_wrapper->err) {
      break;
    }
    utils::print_kafka_message(*message_wrapper, *logger_);
    // Commit offsets on broker for the provided list of partitions
    logger_->log_info("Committing offset: %" PRId64 ".", message_wrapper->offset);
    rd_kafka_commit_message(consumer_.get(), message_wrapper.get(), /* async = */ 0);
  }
  logger_->log_info("Done resetting offset manually.");
}

std::string ConsumeKafka::extract_message(const rd_kafka_message_t& rkmessage) const {
  if (RD_KAFKA_RESP_ERR_NO_ERROR != rkmessage.err) {
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "ConsumeKafka: received error message from broker: " + std::to_string(rkmessage.err) + " " + rd_kafka_err2str(rkmessage.err));
  }
  return { reinterpret_cast<char*>(rkmessage.payload), rkmessage.len };
}

std::vector<std::unique_ptr<rd_kafka_message_t, utils::rd_kafka_message_deleter>> ConsumeKafka::poll_kafka_messages() {
  std::vector<std::unique_ptr<rd_kafka_message_t, utils::rd_kafka_message_deleter>> messages;
  messages.reserve(max_poll_records_);
  const auto start = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::steady_clock::now() - start;
  while (messages.size() < max_poll_records_ && elapsed < max_poll_time_milliseconds_) {
    logger_->log_debug("Polling for new messages for %d milliseconds...", max_poll_time_milliseconds_.count());
    std::unique_ptr<rd_kafka_message_t, utils::rd_kafka_message_deleter>
      message { rd_kafka_consumer_poll(consumer_.get(), std::chrono::duration_cast<std::chrono::milliseconds>(max_poll_time_milliseconds_ - elapsed).count()), utils::rd_kafka_message_deleter() };
    if (!message) {
      break;
    }
    if (RD_KAFKA_RESP_ERR_NO_ERROR != message->err) {
      logger_->log_error("Received message with error %d: %s", message->err, rd_kafka_err2str(message->err));
      break;
    }
    utils::print_kafka_message(*message, *logger_);
    messages.emplace_back(std::move(message));
    elapsed = std::chrono::steady_clock::now() - start;
  }
  return messages;
}

utils::KafkaEncoding ConsumeKafka::key_attr_encoding_attr_to_enum() const {
  if (utils::StringUtils::equalsIgnoreCase(key_attribute_encoding_, KEY_ATTR_ENCODING_UTF_8)) {
    return utils::KafkaEncoding::UTF8;
  }
  if (utils::StringUtils::equalsIgnoreCase(key_attribute_encoding_, KEY_ATTR_ENCODING_HEX)) {
    return utils::KafkaEncoding::HEX;
  }
  throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "\"Key Attribute Encoding\" property not recognized.");
}

utils::KafkaEncoding ConsumeKafka::message_header_encoding_attr_to_enum() const {
  if (utils::StringUtils::equalsIgnoreCase(message_header_encoding_, MSG_HEADER_ENCODING_UTF_8)) {
    return utils::KafkaEncoding::UTF8;
  }
  if (utils::StringUtils::equalsIgnoreCase(message_header_encoding_, MSG_HEADER_ENCODING_HEX)) {
    return utils::KafkaEncoding::HEX;
  }
  throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "\"Message Header Encoding\" property not recognized.");
}

std::string ConsumeKafka::resolve_duplicate_headers(const std::vector<std::string>& matching_headers) const {
  if (MSG_HEADER_KEEP_FIRST == duplicate_header_handling_) {
    return matching_headers.front();
  }
  if (MSG_HEADER_KEEP_LATEST == duplicate_header_handling_) {
    return matching_headers.back();
  }
  if (MSG_HEADER_COMMA_SEPARATED_MERGE == duplicate_header_handling_) {
    return utils::StringUtils::join(", ", matching_headers);
  }
  throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "\"Duplicate Header Handling\" property not recognized.");
}

std::vector<std::string> ConsumeKafka::get_matching_headers(const rd_kafka_message_t& message, const std::string& header_name) const {
  // Headers fetched this way are freed when rd_kafka_message_destroy is called
  // Detaching them using rd_kafka_message_detach_headers does not seem to work
  rd_kafka_headers_t* headers_raw;
  const rd_kafka_resp_err_t get_header_response = rd_kafka_message_headers(&message, &headers_raw);
  if (RD_KAFKA_RESP_ERR__NOENT == get_header_response) {
    return {};
  }
  if (RD_KAFKA_RESP_ERR_NO_ERROR != get_header_response) {
    logger_->log_error("Failed to fetch message headers: %d: %s", rd_kafka_last_error(), rd_kafka_err2str(rd_kafka_last_error()));
  }
  std::vector<std::string> matching_headers;
  for (std::size_t header_idx = 0;; ++header_idx) {
    const char* value;  // Not to be freed
    std::size_t size;
    if (RD_KAFKA_RESP_ERR_NO_ERROR != rd_kafka_header_get(headers_raw, header_idx, header_name.c_str(), (const void**)(&value), &size)) {
      break;
    }
    if (size < 200) {
      logger_->log_debug("%.*s", static_cast<int>(size), value);
    } else {
      logger_->log_debug("%.*s...", 200, value);
    }
    matching_headers.emplace_back(value, size);
  }
  return matching_headers;
}

std::vector<std::pair<std::string, std::string>> ConsumeKafka::get_flowfile_attributes_from_message_header(const rd_kafka_message_t& message) const {
  std::vector<std::pair<std::string, std::string>> attributes_from_headers;
  for (const std::string& header_name : headers_to_add_as_attributes_) {
    const std::vector<std::string> matching_headers = get_matching_headers(message, header_name);
    if (matching_headers.size()) {
      attributes_from_headers.emplace_back(header_name, utils::get_encoded_string(resolve_duplicate_headers(matching_headers), message_header_encoding_attr_to_enum()));
    }
  }
  return attributes_from_headers;
}

void ConsumeKafka::add_kafka_attributes_to_flowfile(std::shared_ptr<FlowFileRecord>& flow_file, const rd_kafka_message_t& message) const {
  // We do not currently support batching messages into a single flowfile
  flow_file->setAttribute(KAFKA_COUNT_ATTR, "1");
  const utils::optional<std::string> message_key = utils::get_encoded_message_key(message, key_attr_encoding_attr_to_enum());
  if (message_key) {
    flow_file->setAttribute(KAFKA_MESSAGE_KEY_ATTR, message_key.value());
  }
  flow_file->setAttribute(KAFKA_OFFSET_ATTR, std::to_string(message.offset));
  flow_file->setAttribute(KAFKA_PARTITION_ATTR, std::to_string(message.partition));
  flow_file->setAttribute(KAFKA_TOPIC_ATTR, rd_kafka_topic_name(message.rkt));
}

utils::optional<std::vector<std::shared_ptr<FlowFileRecord>>> ConsumeKafka::transform_pending_messages_into_flowfiles(core::ProcessSession& session) const {
  std::vector<std::shared_ptr<FlowFileRecord>> flow_files_created;
  for (const auto& message : pending_messages_) {
    std::string message_content = extract_message(*message);
    std::vector<std::pair<std::string, std::string>> attributes_from_headers = get_flowfile_attributes_from_message_header(*message);
    std::vector<std::string> split_message{ message_demarcator_.size() ?
      utils::StringUtils::split(message_content, message_demarcator_) :
      std::vector<std::string>{ message_content }};
    for (auto& flowfile_content : split_message) {
      std::shared_ptr<FlowFileRecord> flow_file = std::static_pointer_cast<FlowFileRecord>(session.create());
      if (flow_file == nullptr) {
        logger_->log_error("Failed to create flowfile.");
        // Either transform all flowfiles or none
        return {};
      }
      // flowfile content is consumed here
      WriteCallback stream_writer_callback(&flowfile_content[0], flowfile_content.size());
      session.write(flow_file, &stream_writer_callback);
      for (const auto& kv : attributes_from_headers) {
        flow_file->setAttribute(kv.first, kv.second);
      }
      add_kafka_attributes_to_flowfile(flow_file, *message);
      flow_files_created.emplace_back(std::move(flow_file));
    }
  }
  return { flow_files_created };
}


void ConsumeKafka::process_pending_messages(core::ProcessSession& session) {
  utils::optional<std::vector<std::shared_ptr<FlowFileRecord>>> flow_files_created = transform_pending_messages_into_flowfiles(session);
  if (!flow_files_created) {
    return;
  }
  for (const auto& flow_file : flow_files_created.value()) {
    session.transfer(flow_file, Success);
  }
  session.commit();
  // Commit the offset from the latest message only
  if (RD_KAFKA_RESP_ERR_NO_ERROR != rd_kafka_commit_message(consumer_.get(), pending_messages_.back().get(), /* async = */ 0)) {
    logger_->log_error("Committing offset failed.");
  }
  pending_messages_.clear();
}

void ConsumeKafka::onTrigger(core::ProcessContext* /* context */, core::ProcessSession* session) {
  std::unique_lock<std::mutex> lock(do_not_call_on_trigger_concurrently_);
  logger_->log_debug("ConsumeKafka onTrigger");

  if (pending_messages_.size()) {
    process_pending_messages(*session);
    return;
  }

  pending_messages_ = poll_kafka_messages();
  if (pending_messages_.empty()) {
    return;
  }
  process_pending_messages(*session);
}

int64_t ConsumeKafka::WriteCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  if (!data_) return 0;
  const auto write_ret = stream->write(data_, dataSize_);
  if (io::isError(write_ret)) return -1;
  return gsl::narrow<int64_t>(write_ret);
}

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
