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
#include <ranges>

#include "minifi-cpp/core/FlowFile.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "core/Resource.h"
#include "utils/OptionalUtils.h"
#include "utils/AttributeErrors.h"
#include "utils/ProcessorConfigUtils.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/expected.h"

using namespace std::literals::chrono_literals;

template<>
struct std::hash<org::apache::nifi::minifi::processors::ConsumeKafka::KafkaMessageLocation> {
  size_t operator()(const org::apache::nifi::minifi::processors::ConsumeKafka::KafkaMessageLocation& message_location) const noexcept {
    return org::apache::nifi::minifi::utils::hash_combine(std::hash<std::string_view>{}(message_location.topic),
        std::hash<int32_t>{}(message_location.partition));
  }
};

namespace org::apache::nifi::minifi::processors {
// The upper limit for Max Poll Time is 4 seconds. This is because Watchdog would potentially start
// reporting issues with the processor health otherwise
bool consume_kafka::ConsumeKafkaMaxPollTimePropertyValidator::validate(const std::string_view input) const {
  const auto parsed_time = parsing::parseDurationMinMax<std::chrono::nanoseconds>(input, 0ms, 4s);
  return parsed_time.has_value();
}

void ConsumeKafka::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ConsumeKafka::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  // Required properties
  topic_names_ = utils::string::splitAndTrim(utils::parseProperty(context, TopicNames), ",");
  topic_name_format_ = utils::parseEnumProperty<consume_kafka::TopicNameFormatEnum>(context, TopicNameFormat);
  commit_policy_ = utils::parseEnumProperty<consume_kafka::CommitPolicyEnum>(context, CommitPolicy);
  key_attribute_encoding_ = utils::parseEnumProperty<utils::KafkaEncoding>(context, KeyAttributeEncoding);
  message_header_encoding_ = utils::parseEnumProperty<utils::KafkaEncoding>(context, MessageHeaderEncoding);
  duplicate_header_handling_ = utils::parseEnumProperty<consume_kafka::MessageHeaderPolicyEnum>(context, DuplicateHeaderHandling);
  max_poll_time_milliseconds_ = utils::parseDurationProperty(context, MaxPollTime);
  max_poll_records_ = gsl::narrow<uint32_t>(utils::parseU64Property(context, MaxPollRecords));

  // Optional properties
  message_demarcator_ = utils::parseOptionalProperty(context, MessageDemarcator);
  headers_to_add_as_attributes_ = parseOptionalProperty(context, HeadersToAddAsAttributes)
      | utils::transform([](const std::string& headers_to_add_str) { return utils::string::splitAndTrim(headers_to_add_str, ","); });
  if (message_demarcator_ && headers_to_add_as_attributes_) {
    logger_->log_error("Message merging with header extraction is not yet supported");
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Message merging with header extraction is not yet supported");
  }

  configureNewConnection(context);
  if (commit_policy_ == consume_kafka::CommitPolicyEnum::CommitFromIncomingFlowFiles) {
    setTriggerWhenEmpty(true);
  } else if (context.hasIncomingConnections()) {
    logger_->log_error("Incoming connections are not allowed with {}", magic_enum::enum_name(commit_policy_));
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Incoming connections are not allowed with {}", magic_enum::enum_name(commit_policy_)));
  }
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
      if (logger->should_log(core::logging::LOG_LEVEL::debug)) { utils::print_topics_list(*logger, *partitions); }
      assign_error = rd_kafka_assign(rk, partitions);
      break;

    case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
      logger->log_debug("revoked:");
      rd_kafka_commit(rk, partitions, /* async = */ 0);  // Sync commit, maybe unnecessary
      if (logger->should_log(core::logging::LOG_LEVEL::debug)) { utils::print_topics_list(*logger, *partitions); }
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

void ConsumeKafka::createTopicPartitionList() {
  kf_topic_partition_list_ = utils::rd_kafka_topic_partition_list_unique_ptr{
      rd_kafka_topic_partition_list_new(gsl::narrow<int>(topic_names_.size()))};

  if (topic_name_format_ == consume_kafka::TopicNameFormatEnum::Patterns) {
    for (const std::string& topic: topic_names_) {
      const std::string regex_format = "^" + topic;
      rd_kafka_topic_partition_list_add(kf_topic_partition_list_.get(), regex_format.c_str(), RD_KAFKA_PARTITION_UA);
    }
  } else {
    for (const std::string& topic: topic_names_) {
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

void ConsumeKafka::extendConfigFromDynamicProperties(const core::ProcessContext& context) const {
  using utils::setKafkaConfigurationField;

  const std::vector<std::string> dynamic_prop_keys = context.getDynamicPropertyKeys();
  if (dynamic_prop_keys.empty()) { return; }
  logger_->log_info("Loading {} extra kafka configuration fields from ConsumeKafka dynamic properties:", dynamic_prop_keys.size());
  for (const std::string& key: dynamic_prop_keys) {
    std::string value = context.getDynamicProperty(key)
        | utils::orThrow(fmt::format("This shouldn't happen, dynamic property {} is expected because we just queried the list of dynamic properties", key));
    logger_->log_info("{}: {}", key.c_str(), value.c_str());
    setKafkaConfigurationField(*conf_, key, value);
  }
}

void ConsumeKafka::configureNewConnection(core::ProcessContext& context) {
  using utils::setKafkaConfigurationField;

  conf_ = {rd_kafka_conf_new(), utils::rd_kafka_conf_deleter()};
  if (conf_ == nullptr) { throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to create rd_kafka_conf_t object"); }

  // Set rebalance callback for use with coordinated consumer group balancing
  // Rebalance handlers are needed for the initial configuration of the consumer
  // If they are not set, offset reset is ignored and polling produces messages
  // Registering a rebalance_cb turns off librdkafka's automatic partition assignment/revocation and instead delegates that
  // responsibility to the application's rebalance_cb.
  if (commit_policy_ != consume_kafka::CommitPolicyEnum::CommitFromIncomingFlowFiles) {
    rd_kafka_conf_set_rebalance_cb(conf_.get(), rebalance_cb);
  }

  // Uncomment this for librdkafka debug logs:
  // logger_->log_info("Enabling all debug logs for kafka consumer.");
  // setKafkaConfigurationField(*conf_, "debug", "all");

  setKafkaAuthenticationParameters(context, gsl::make_not_null(conf_.get()));

  setKafkaConfigurationField(*conf_, "bootstrap.servers", utils::parseProperty(context, KafkaBrokers));
  setKafkaConfigurationField(*conf_, "allow.auto.create.topics", "true");
  setKafkaConfigurationField(*conf_,
      "auto.offset.reset",
      std::string(magic_enum::enum_name(utils::parseEnumProperty<consume_kafka::OffsetResetPolicyEnum>(context, OffsetReset))));
  setKafkaConfigurationField(*conf_, "enable.auto.commit", std::to_string(commit_policy_ == consume_kafka::CommitPolicyEnum::AutoCommit));
  setKafkaConfigurationField(*conf_, "enable.auto.offset.store", std::to_string(commit_policy_ == consume_kafka::CommitPolicyEnum::AutoCommit));
  setKafkaConfigurationField(*conf_, "isolation.level", utils::parseBoolProperty(context, HonorTransactions) ? "read_committed" : "read_uncommitted");
  setKafkaConfigurationField(*conf_, "group.id", utils::parseProperty(context, GroupID));
  setKafkaConfigurationField(*conf_, "client.id", this->getUUIDStr());
  setKafkaConfigurationField(*conf_, "session.timeout.ms", std::to_string(utils::parseDurationProperty(context, SessionTimeout).count()));
  // Twice the default, arbitrarily chosen
  setKafkaConfigurationField(*conf_, "max.poll.interval.ms", "600000");

  extendConfigFromDynamicProperties(context);

  std::array<char, 512U> errstr{};
  consumer_ = {rd_kafka_new(RD_KAFKA_CONSUMER, conf_.release(), errstr.data(), errstr.size()), utils::rd_kafka_consumer_deleter()};
  if (consumer_ == nullptr) {
    const std::string error_msg{errstr.data()};
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to create Kafka consumer " + error_msg);
  }

  createTopicPartitionList();

  // Changing the partition list should happen only as part as the initialization of offsets
  // a function like `rd_kafka_position()` might have unexpected effects
  // for instance when a consumer gets assigned a partition it used to
  // consume at an earlier rebalance.
  //
  // As far as I understand, instead of rd_kafka_position() an rd_kafka_committed() call if preferred here,
  // as it properly fetches offsets from the broker
  if (const auto retrieved_committed = rd_kafka_committed(consumer_.get(), kf_topic_partition_list_.get(), METADATA_COMMUNICATIONS_TIMEOUT_MS); RD_KAFKA_RESP_ERR_NO_ERROR != retrieved_committed) {
    logger_->log_error("Retrieving committed offsets for topics+partitions failed {}: {}",
      magic_enum::enum_underlying(retrieved_committed),
      rd_kafka_err2str(retrieved_committed));
  }

  if (rd_kafka_resp_err_t poll_set_consumer_response = rd_kafka_poll_set_consumer(consumer_.get()); RD_KAFKA_RESP_ERR_NO_ERROR != poll_set_consumer_response) {
    logger_->log_error("rd_kafka_poll_set_consumer error {}: {}",
        magic_enum::enum_underlying(poll_set_consumer_response),
        rd_kafka_err2str(poll_set_consumer_response));
  }
}

std::string ConsumeKafka::extractMessage(const rd_kafka_message_t& rkmessage) {
  if (RD_KAFKA_RESP_ERR_NO_ERROR != rkmessage.err) {
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION,
        "ConsumeKafka: received error message from broker: " + std::to_string(rkmessage.err) + " " + rd_kafka_err2str(rkmessage.err));
  }
  return {static_cast<char*>(rkmessage.payload), rkmessage.len};
}

std::unordered_map<ConsumeKafka::KafkaMessageLocation, ConsumeKafka::MessageBundle> ConsumeKafka::pollKafkaMessages() {
  std::unordered_map<KafkaMessageLocation, MessageBundle> message_bundles;
  const auto start = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::steady_clock::now() - start;
  size_t message_count = 0;
  while (message_count < max_poll_records_ && elapsed < max_poll_time_milliseconds_) {
    logger_->log_debug("Polling for new messages for {}...", max_poll_time_milliseconds_);
    const auto timeout_ms = gsl::narrow<int>(std::chrono::duration_cast<std::chrono::milliseconds>(max_poll_time_milliseconds_ - elapsed).count());
    utils::rd_kafka_message_unique_ptr message{rd_kafka_consumer_poll(consumer_.get(), timeout_ms)};
    if (!message) { break; }
    if (RD_KAFKA_RESP_ERR_NO_ERROR != message->err) {
      logger_->log_error("Received message with error {}: {}", magic_enum::enum_underlying(message->err), rd_kafka_err2str(message->err));
      break;
    }
    const std::string_view topic_name = rd_kafka_topic_name(message->rkt);
    const int32_t partition = message->partition;

    message_bundles[KafkaMessageLocation{std::string(topic_name), partition}].pushBack(std::move(message));
    elapsed = std::chrono::steady_clock::now() - start;
    ++message_count;
  }
  return message_bundles;
}

std::string ConsumeKafka::resolve_duplicate_headers(const std::vector<std::string>& matching_headers) const {
  switch (duplicate_header_handling_) {
    case consume_kafka::MessageHeaderPolicyEnum::KEEP_FIRST: return matching_headers.front();
    case consume_kafka::MessageHeaderPolicyEnum::KEEP_LATEST: return matching_headers.back();
    case consume_kafka::MessageHeaderPolicyEnum::COMMA_SEPARATED_MERGE: return utils::string::join(", ", matching_headers);
    default: throw Exception(PROCESSOR_EXCEPTION, "\"Duplicate Header Handling\" property not recognized.");
  }
}

std::vector<std::string> ConsumeKafka::get_matching_headers(const rd_kafka_message_t& message, const std::string& header_name) const {
  // Headers fetched this way are freed when rd_kafka_message_destroy is called
  // Detaching them using rd_kafka_message_detach_headers does not seem to work
  rd_kafka_headers_t* headers_raw = nullptr;
  const rd_kafka_resp_err_t get_header_response = rd_kafka_message_headers(&message, &headers_raw);
  if (RD_KAFKA_RESP_ERR__NOENT == get_header_response) { return {}; }
  if (RD_KAFKA_RESP_ERR_NO_ERROR != get_header_response) {
    logger_->log_error("Failed to fetch message headers: {}: {}",
        magic_enum::enum_underlying(rd_kafka_last_error()),
        rd_kafka_err2str(rd_kafka_last_error()));
  }
  std::vector<std::string> matching_headers;
  for (std::size_t header_idx = 0;; ++header_idx) {
    const char* value = nullptr;  // Not to be freed
    std::size_t size = 0;
    if (RD_KAFKA_RESP_ERR_NO_ERROR !=
        rd_kafka_header_get(headers_raw, header_idx, header_name.c_str(), reinterpret_cast<const void**>(&value), &size)) {
      break;
    }
    matching_headers.emplace_back(value, size);
  }
  return matching_headers;
}

std::vector<std::pair<std::string, std::string>> ConsumeKafka::getFlowFilesAttributesFromMessageHeaders(const rd_kafka_message_t& message) const {
  gsl_Assert(headers_to_add_as_attributes_);
  std::vector<std::pair<std::string, std::string>> attributes_from_headers;
  for (const std::string& header_name: *headers_to_add_as_attributes_) {
    const std::vector<std::string> matching_headers = get_matching_headers(message, header_name);
    if (!matching_headers.empty()) {
      attributes_from_headers.emplace_back(header_name,
          utils::get_encoded_string(resolve_duplicate_headers(matching_headers), message_header_encoding_));
    }
  }
  return attributes_from_headers;
}

void ConsumeKafka::addAttributesToSingleMessageFlowFile(core::FlowFile& flow_file, const rd_kafka_message_t& message) const {
  flow_file.setAttribute(KafkaCountAttribute.name, "1");
  if (const auto message_key = get_encoded_message_key(message, key_attribute_encoding_)) {
    flow_file.setAttribute(KafkaKeyAttribute.name, *message_key);
  }
  flow_file.setAttribute(KafkaOffsetAttribute.name, std::to_string(message.offset));
  flow_file.setAttribute(KafkaPartitionAttribute.name, std::to_string(message.partition));
  flow_file.setAttribute(KafkaTopicAttribute.name, rd_kafka_topic_name(message.rkt));
  if (headers_to_add_as_attributes_) {
    for (const auto& [attr_key, attr_value]: getFlowFilesAttributesFromMessageHeaders(message)) { flow_file.setAttribute(attr_key, attr_value); }
  }
}

void ConsumeKafka::addAttributesToMessageBundleFlowFile(core::FlowFile& flow_file, const MessageBundle& message_bundle) const {
  gsl_Assert(!headers_to_add_as_attributes_);
  flow_file.setAttribute(KafkaCountAttribute.name, std::to_string(message_bundle.getMessages().size()));
  flow_file.setAttribute(KafkaOffsetAttribute.name, std::to_string(message_bundle.getLargestOffset()));
  flow_file.setAttribute(KafkaPartitionAttribute.name, std::to_string(message_bundle.getMessages().front()->partition));
  flow_file.setAttribute(KafkaTopicAttribute.name, rd_kafka_topic_name(message_bundle.getMessages().front()->rkt));
}

void ConsumeKafka::commitOffsetsFromMessages(const std::unordered_map<KafkaMessageLocation, MessageBundle>& message_bundles) const {
  if (message_bundles.empty()) { return; }

  const auto partitions = utils::rd_kafka_topic_partition_list_unique_ptr{rd_kafka_topic_partition_list_new(gsl::narrow<int>(message_bundles.size()))};

  for (const auto& [location, message_bundle] : message_bundles) {
    logger_->log_debug("Message bundle offsets: {} {} {}", location.topic, location.partition, message_bundle.getLargestOffset());
    rd_kafka_topic_partition_list_add(partitions.get(), location.topic.data(), location.partition)->offset = message_bundle.getLargestOffset() + 1;
  }

  const auto commit_err_code = rd_kafka_commit(consumer_.get(), partitions.get(), 0);

  if (RD_KAFKA_RESP_ERR_NO_ERROR != commit_err_code) {
    logger_->log_error("Committing offset failed: {}: {}",
        magic_enum::enum_underlying(commit_err_code), rd_kafka_err2str(commit_err_code));
  }
}

void ConsumeKafka::commitOffsetsFromIncomingFlowFiles(core::ProcessSession& session) const {
  std::vector<std::pair<std::shared_ptr<core::FlowFile>, core::Relationship>> flow_files;
  while (auto ff = session.get()) { flow_files.push_back({ff, Committed}); }
  if (flow_files.empty()) { return; }

  std::unordered_map<KafkaMessageLocation, int64_t> max_offsets;
  for (auto& [flow_file, relationship]: std::ranges::reverse_view(flow_files)) {
    auto topic_name = flow_file->getAttribute(KafkaTopicAttribute.name)
        | utils::toExpected(make_error_code(core::AttributeErrorCode::MissingAttribute));
    const auto offset = flow_file->getAttribute(KafkaOffsetAttribute.name)
        | utils::toExpected(make_error_code(core::AttributeErrorCode::MissingAttribute))
        | utils::andThen(parsing::parseIntegral<int64_t>);
    const auto partition = flow_file->getAttribute(KafkaPartitionAttribute.name)
        | utils::toExpected(make_error_code(core::AttributeErrorCode::MissingAttribute))
        | utils::andThen(parsing::parseIntegral<int32_t>);
    if (!topic_name || !offset || !partition) {
      logger_->log_error("Insufficient data for setting offsets. {} : {}, {} : {}, {} : {}",
          KafkaTopicAttribute.name,
          topic_name,
          KafkaOffsetAttribute.name,
          offset,
          KafkaPartitionAttribute.name,
          partition);
      relationship = Failure;
    }
    int64_t& curr_offset = max_offsets[KafkaMessageLocation{std::move(topic_name.value()), *partition}];
    curr_offset = std::max(curr_offset, *offset);
  }

  const auto partitions = utils::rd_kafka_topic_partition_list_unique_ptr{rd_kafka_topic_partition_list_new(gsl::narrow<int>(max_offsets.size()))};

  for (const auto& [location, max_offset] : max_offsets) {
    rd_kafka_topic_partition_list_add(partitions.get(), location.topic.data(), location.partition)->offset = max_offset + 1;
  }
  const auto commit_res = rd_kafka_commit(consumer_.get(), partitions.get(), 0);
  switch (commit_res) {
    case RD_KAFKA_RESP_ERR_NO_ERROR: {
      logger_->log_debug("Commit successfully from {} flowfiles", flow_files.size());
      break;
    }
    case RD_KAFKA_RESP_ERR_OFFSET_OUT_OF_RANGE: {
      logger_->log_info("{}, continuing", rd_kafka_err2str(RD_KAFKA_RESP_ERR_OFFSET_OUT_OF_RANGE));
      break;
    }
    case RD_KAFKA_RESP_ERR__NO_OFFSET: {
      logger_->log_info("{}, continuing", rd_kafka_err2str(RD_KAFKA_RESP_ERR__NO_OFFSET));
      break;
    }
    default: {
      logger_->log_error("Committing offset failed: {}: {}", magic_enum::enum_underlying(commit_res), rd_kafka_err2str(commit_res));
      throw Exception(PROCESS_SESSION_EXCEPTION, fmt::format("Committing offset failed: {}: {}", magic_enum::enum_underlying(commit_res), rd_kafka_err2str(commit_res)));
    }
  }
  for (const auto& [ff, relationship]: flow_files) { session.transfer(ff, relationship); }
}

void ConsumeKafka::processMessages(core::ProcessSession& session, const std::unordered_map<KafkaMessageLocation, MessageBundle>& message_bundles) const {
  for (const auto& msg_bundle: message_bundles | std::views::values) {
    for (const auto& message: msg_bundle.getMessages()) {
      std::string message_content = extractMessage(*message);
      auto flow_file = session.create();
      session.writeBuffer(flow_file, message_content);
      addAttributesToSingleMessageFlowFile(*flow_file, *message);
      session.transfer(flow_file, Success);
    }
  }
  session.commit();
  if (commit_policy_ == consume_kafka::CommitPolicyEnum::CommitAfterBatch) { commitOffsetsFromMessages(message_bundles); }
}

void ConsumeKafka::processMessageBundles(core::ProcessSession& session,
    const std::unordered_map<KafkaMessageLocation, MessageBundle>& message_bundles, const std::string_view message_demarcator) const {
  for (const auto& msg_bundle: message_bundles | std::views::values) {
    auto flow_file = session.create();
    auto merged_message_content = utils::string::join(message_demarcator, msg_bundle.getMessages(), [](const auto& message) {
      return extractMessage(*message);
    });
    session.writeBuffer(flow_file, merged_message_content);
    addAttributesToMessageBundleFlowFile(*flow_file, msg_bundle);
    session.transfer(flow_file, Success);
  }
}

void ConsumeKafka::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  if (commit_policy_ == consume_kafka::CommitPolicyEnum::CommitFromIncomingFlowFiles) { commitOffsetsFromIncomingFlowFiles(session); }
  const auto message_bundles = pollKafkaMessages();
  if (message_bundles.empty()) {
    logger_->log_debug("No new messages");
    return;
  }
  if (!message_demarcator_) {
    processMessages(session, message_bundles);
  } else {
    processMessageBundles(session, message_bundles, *message_demarcator_);
  }
}

REGISTER_RESOURCE(ConsumeKafka, Processor);
}  // namespace org::apache::nifi::minifi::processors
