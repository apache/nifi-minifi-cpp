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

#include "core/ProcessSession.h"
#include "core/PropertyType.h"
#include "core/Resource.h"
#include "FlowFileRecord.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/gsl.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi {
namespace core {
// The upper limit for Max Poll Time is 4 seconds. This is because Watchdog would potentially start
// reporting issues with the processor health otherwise
ValidationResult ConsumeKafkaMaxPollTimePropertyType::validate(const std::string& subject, const std::string& input) const {
  auto parsed_value = utils::timeutils::StringToDuration<std::chrono::milliseconds>(input);
  bool is_valid = parsed_value.has_value() && 0ms < *parsed_value && *parsed_value <= 4s;
  return ValidationResult{.valid = is_valid, .subject = subject, .input = input};
}
}  // namespace core

namespace processors {

void ConsumeKafka::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ConsumeKafka::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  // Required properties
  kafka_brokers_                = utils::getRequiredPropertyOrThrow(context, KafkaBrokers.name);
  topic_names_                  = utils::listFromRequiredCommaSeparatedProperty(context, TopicNames.name);
  topic_name_format_            = utils::getRequiredPropertyOrThrow(context, TopicNameFormat.name);
  honor_transactions_           = utils::parseBooleanPropertyOrThrow(context, HonorTransactions.name);
  group_id_                     = utils::getRequiredPropertyOrThrow(context, GroupID.name);
  offset_reset_                 = utils::getRequiredPropertyOrThrow(context, OffsetReset.name);
  key_attribute_encoding_       = utils::getRequiredPropertyOrThrow(context, KeyAttributeEncoding.name);
  max_poll_time_milliseconds_   = utils::parseTimePropertyMSOrThrow(context, MaxPollTime.name);
  session_timeout_milliseconds_ = utils::parseTimePropertyMSOrThrow(context, SessionTimeout.name);

  // Optional properties
  context.getProperty(MessageDemarcator, message_demarcator_);
  context.getProperty(MessageHeaderEncoding, message_header_encoding_);
  context.getProperty(DuplicateHeaderHandling, duplicate_header_handling_);

  headers_to_add_as_attributes_ = utils::listFromCommaSeparatedProperty(context, HeadersToAddAsAttributes.name);
  max_poll_records_ = gsl::narrow<std::size_t>(context.getProperty<uint64_t>(MaxPollRecords).value_or(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE.parse(DEFAULT_MAX_POLL_RECORDS)));

  if (!utils::string::equalsIgnoreCase(KEY_ATTR_ENCODING_UTF_8, key_attribute_encoding_) && !utils::string::equalsIgnoreCase(KEY_ATTR_ENCODING_HEX, key_attribute_encoding_)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Unsupported key attribute encoding: " + key_attribute_encoding_);
  }

  if (!utils::string::equalsIgnoreCase(MSG_HEADER_ENCODING_UTF_8, message_header_encoding_) && !utils::string::equalsIgnoreCase(MSG_HEADER_ENCODING_HEX, message_header_encoding_)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Unsupported message header encoding: " + key_attribute_encoding_);
  }

  configure_new_connection(context);
}

namespace {
void rebalance_cb(rd_kafka_t* rk, rd_kafka_resp_err_t trigger, rd_kafka_topic_partition_list_t* partitions, void* /*opaque*/) {
  // Cooperative, incremental assignment is not supported in the current librdkafka version
  std::shared_ptr<core::logging::Logger> logger{core::logging::LoggerFactory<ConsumeKafka>::getLogger()};
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
      assign_error = rd_kafka_assign(rk, nullptr);
      break;

    default:
      logger->log_debug("failed: {}", rd_kafka_err2str(trigger));
      assign_error = rd_kafka_assign(rk, nullptr);
      break;
  }
  logger->log_debug("assign failure: {}", rd_kafka_err2str(assign_error));
}
}  // namespace

void ConsumeKafka::create_topic_partition_list() {
  kf_topic_partition_list_ = { rd_kafka_topic_partition_list_new(gsl::narrow<int>(topic_names_.size())), utils::rd_kafka_topic_partition_list_deleter() };

  // On subscriptions any topics prefixed with ^ will be regex matched
  if (utils::string::equalsIgnoreCase(TOPIC_FORMAT_PATTERNS, topic_name_format_)) {
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
    logger_->log_error("rd_kafka_subscribe error {}: {}", magic_enum::enum_underlying(subscribe_response), rd_kafka_err2str(subscribe_response));
  }
}

void ConsumeKafka::extend_config_from_dynamic_properties(const core::ProcessContext& context) {
  using utils::setKafkaConfigurationField;

  const std::vector<std::string> dynamic_prop_keys = context.getDynamicPropertyKeys();
  if (dynamic_prop_keys.empty()) {
    return;
  }
  logger_->log_info("Loading {} extra kafka configuration fields from ConsumeKafka dynamic properties:", dynamic_prop_keys.size());
  for (const std::string& key : dynamic_prop_keys) {
    std::string value;
    gsl_Expects(context.getDynamicProperty(key, value));
    logger_->log_info("{}: {}", key.c_str(), value.c_str());
    setKafkaConfigurationField(*conf_, key, value);
  }
}

void ConsumeKafka::configure_new_connection(core::ProcessContext& context) {
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
  // logger_->log_info("Enabling all debug logs for kafka consumer.");
  // setKafkaConfigurationField(*conf_, "debug", "all");

  setKafkaAuthenticationParameters(context, gsl::make_not_null(conf_.get()));

  setKafkaConfigurationField(*conf_, "bootstrap.servers", kafka_brokers_);
  setKafkaConfigurationField(*conf_, "allow.auto.create.topics", "true");
  setKafkaConfigurationField(*conf_, "auto.offset.reset", offset_reset_);
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
    logger_->log_error("rd_kafka_poll_set_consumer error {}: {}", magic_enum::enum_underlying(poll_set_consumer_response), rd_kafka_err2str(poll_set_consumer_response));
  }
}

std::string ConsumeKafka::extract_message(const rd_kafka_message_t& rkmessage) {
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
    logger_->log_debug("Polling for new messages for {}...", max_poll_time_milliseconds_);
    const auto timeout_ms = gsl::narrow<int>(std::chrono::duration_cast<std::chrono::milliseconds>(max_poll_time_milliseconds_ - elapsed).count());
    std::unique_ptr<rd_kafka_message_t, utils::rd_kafka_message_deleter> message{rd_kafka_consumer_poll(consumer_.get(), timeout_ms)};
    if (!message) {
      break;
    }
    if (RD_KAFKA_RESP_ERR_NO_ERROR != message->err) {
      logger_->log_error("Received message with error {}: {}", magic_enum::enum_underlying(message->err), rd_kafka_err2str(message->err));
      break;
    }
    utils::print_kafka_message(*message, *logger_);
    messages.emplace_back(std::move(message));
    elapsed = std::chrono::steady_clock::now() - start;
  }
  return messages;
}

utils::KafkaEncoding ConsumeKafka::key_attr_encoding_attr_to_enum() const {
  if (utils::string::equalsIgnoreCase(key_attribute_encoding_, KEY_ATTR_ENCODING_UTF_8)) {
    return utils::KafkaEncoding::UTF8;
  }
  if (utils::string::equalsIgnoreCase(key_attribute_encoding_, KEY_ATTR_ENCODING_HEX)) {
    return utils::KafkaEncoding::HEX;
  }
  throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "\"Key Attribute Encoding\" property not recognized.");
}

utils::KafkaEncoding ConsumeKafka::message_header_encoding_attr_to_enum() const {
  if (utils::string::equalsIgnoreCase(message_header_encoding_, MSG_HEADER_ENCODING_UTF_8)) {
    return utils::KafkaEncoding::UTF8;
  }
  if (utils::string::equalsIgnoreCase(message_header_encoding_, MSG_HEADER_ENCODING_HEX)) {
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
    return utils::string::join(", ", matching_headers);
  }
  throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "\"Duplicate Header Handling\" property not recognized.");
}

std::vector<std::string> ConsumeKafka::get_matching_headers(const rd_kafka_message_t& message, const std::string& header_name) const {
  // Headers fetched this way are freed when rd_kafka_message_destroy is called
  // Detaching them using rd_kafka_message_detach_headers does not seem to work
  rd_kafka_headers_t* headers_raw = nullptr;
  const rd_kafka_resp_err_t get_header_response = rd_kafka_message_headers(&message, &headers_raw);
  if (RD_KAFKA_RESP_ERR__NOENT == get_header_response) {
    return {};
  }
  if (RD_KAFKA_RESP_ERR_NO_ERROR != get_header_response) {
    logger_->log_error("Failed to fetch message headers: {}: {}", magic_enum::enum_underlying(rd_kafka_last_error()), rd_kafka_err2str(rd_kafka_last_error()));
  }
  std::vector<std::string> matching_headers;
  for (std::size_t header_idx = 0;; ++header_idx) {
    const char* value = nullptr;  // Not to be freed
    std::size_t size = 0;
    if (RD_KAFKA_RESP_ERR_NO_ERROR != rd_kafka_header_get(headers_raw, header_idx, header_name.c_str(), reinterpret_cast<const void**>(&value), &size)) {
      break;
    }
    if (size < 200) {
      logger_->log_debug("{:.{}}", value, size);
    } else {
      logger_->log_debug("{:.{}}...", value, 200);
    }
    matching_headers.emplace_back(value, size);
  }
  return matching_headers;
}

std::vector<std::pair<std::string, std::string>> ConsumeKafka::get_flowfile_attributes_from_message_header(const rd_kafka_message_t& message) const {
  std::vector<std::pair<std::string, std::string>> attributes_from_headers;
  for (const std::string& header_name : headers_to_add_as_attributes_) {
    const std::vector<std::string> matching_headers = get_matching_headers(message, header_name);
    if (!matching_headers.empty()) {
      attributes_from_headers.emplace_back(header_name, utils::get_encoded_string(resolve_duplicate_headers(matching_headers), message_header_encoding_attr_to_enum()));
    }
  }
  return attributes_from_headers;
}

void ConsumeKafka::add_kafka_attributes_to_flowfile(std::shared_ptr<core::FlowFile>& flow_file, const rd_kafka_message_t& message) const {
  // We do not currently support batching messages into a single flowfile
  flow_file->setAttribute(KAFKA_COUNT_ATTR, "1");
  const std::optional<std::string> message_key = utils::get_encoded_message_key(message, key_attr_encoding_attr_to_enum());
  if (message_key) {
    flow_file->setAttribute(KAFKA_MESSAGE_KEY_ATTR, message_key.value());
  }
  flow_file->setAttribute(KAFKA_OFFSET_ATTR, std::to_string(message.offset));
  flow_file->setAttribute(KAFKA_PARTITION_ATTR, std::to_string(message.partition));
  flow_file->setAttribute(KAFKA_TOPIC_ATTR, rd_kafka_topic_name(message.rkt));
}

std::optional<std::vector<std::shared_ptr<core::FlowFile>>> ConsumeKafka::transform_pending_messages_into_flowfiles(core::ProcessSession& session) const {
  std::vector<std::shared_ptr<core::FlowFile>> flow_files_created;
  for (const auto& message : pending_messages_) {
    std::string message_content = extract_message(*message);
    std::vector<std::pair<std::string, std::string>> attributes_from_headers = get_flowfile_attributes_from_message_header(*message);
    std::vector<std::string> split_message{ !message_demarcator_.empty() ?
      utils::string::split(message_content, message_demarcator_) :
      std::vector<std::string>{ message_content }};
    for (auto& flowfile_content : split_message) {
      std::shared_ptr<core::FlowFile> flow_file = session.create();
      if (flow_file == nullptr) {
        logger_->log_error("Failed to create flowfile.");
        // Either transform all flowfiles or none
        return {};
      }
      // flowfile content is consumed here
      session.writeBuffer(flow_file, flowfile_content);
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
  std::optional<std::vector<std::shared_ptr<core::FlowFile>>> flow_files_created = transform_pending_messages_into_flowfiles(session);
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

void ConsumeKafka::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  std::unique_lock<std::mutex> lock(do_not_call_on_trigger_concurrently_);
  logger_->log_debug("ConsumeKafka onTrigger");

  if (!pending_messages_.empty()) {
    process_pending_messages(session);
    return;
  }

  pending_messages_ = poll_kafka_messages();
  if (pending_messages_.empty()) {
    return;
  }
  process_pending_messages(session);
}

REGISTER_RESOURCE(ConsumeKafka, Processor);

}  // namespace processors
}  // namespace org::apache::nifi::minifi
