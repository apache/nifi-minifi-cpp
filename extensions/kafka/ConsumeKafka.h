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

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "KafkaConnection.h"
#include "KafkaProcessorBase.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "core/RelationshipDefinition.h"
#include "core/logging/LoggerFactory.h"
#include "io/StreamPipe.h"
#include "rdkafka.h"
#include "rdkafka_utils.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::processors::consume_kafka {

class ConsumeKafkaMaxPollTimePropertyValidator final : public minifi::core::PropertyValidator {
 public:
  constexpr ~ConsumeKafkaMaxPollTimePropertyValidator() override { }  // NOLINT see comment at grandparent

  [[nodiscard]] bool validate(std::string_view input) const override;
  [[nodiscard]] std::optional<std::string_view> getEquivalentNifiStandardValidatorName() const override { return std::nullopt; }
};

inline constexpr ConsumeKafkaMaxPollTimePropertyValidator CONSUME_KAFKA_MAX_POLL_TIME_TYPE{};

enum class CommitPolicyEnum { NoCommit, AutoCommit, CommitAfterBatch, CommitFromIncomingFlowFiles };

enum class OffsetResetPolicyEnum { earliest, latest, none };

enum class TopicNameFormatEnum { Names, Patterns };

enum class MessageHeaderPolicyEnum { KEEP_FIRST, KEEP_LATEST, COMMA_SEPARATED_MERGE };

}  // namespace org::apache::nifi::minifi::processors::consume_kafka

namespace magic_enum::customize {
using org::apache::nifi::minifi::processors::consume_kafka::MessageHeaderPolicyEnum;
using org::apache::nifi::minifi::processors::consume_kafka::CommitPolicyEnum;

template<>
constexpr customize_t enum_name<CommitPolicyEnum>(const CommitPolicyEnum value) noexcept {
    switch (value) {
        case CommitPolicyEnum::NoCommit: return "No Commit";
        case CommitPolicyEnum::AutoCommit: return "Auto Commit";
        case CommitPolicyEnum::CommitAfterBatch: return "Commit After Batch";
        case CommitPolicyEnum::CommitFromIncomingFlowFiles: return "Commit from incoming flowfiles";
        default: return invalid_tag;
    }
}


template<>
constexpr customize_t enum_name<MessageHeaderPolicyEnum>(const MessageHeaderPolicyEnum value) noexcept {
  switch (value) {
    case MessageHeaderPolicyEnum::KEEP_FIRST: return "Keep First";
    case MessageHeaderPolicyEnum::KEEP_LATEST: return "Keep Latest";
    case MessageHeaderPolicyEnum::COMMA_SEPARATED_MERGE: return "Comma-separated Merge";
    default: return invalid_tag;
  }
}

}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::processors {

class ConsumeKafka final : public KafkaProcessorBase {
 public:
  static constexpr std::string_view DEFAULT_MAX_POLL_RECORDS = "10000";
  static constexpr std::string_view DEFAULT_MAX_POLL_TIME = "4 seconds";

  static constexpr std::size_t METADATA_COMMUNICATIONS_TIMEOUT_MS{60000};

  EXTENSIONAPI static constexpr const char* Description =
      "Consumes messages from Apache Kafka and transform them into MiNiFi FlowFiles. "
      "The application should make sure that the processor is triggered at regular intervals, even if no messages are expected, "
      "to serve any queued callbacks waiting to be called. Rebalancing can also only happen on trigger.";

  EXTENSIONAPI static constexpr auto KafkaBrokers =
      core::PropertyDefinitionBuilder<>::createProperty("Kafka Brokers")
          .withDescription("A comma-separated list of known Kafka Brokers in the format <host>:<port>.")
          .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
          .withDefaultValue("localhost:9092")
          .supportsExpressionLanguage(true)
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto TopicNames =
      core::PropertyDefinitionBuilder<>::createProperty("Topic Names")
          .withDescription("The name of the Kafka Topic(s) to pull from. Multiple topic names are supported as a comma separated list.")
          .supportsExpressionLanguage(true)
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto TopicNameFormat =
      core::PropertyDefinitionBuilder<2>::createProperty("Topic Name Format")
          .withDescription(
              "Specifies whether the Topic(s) provided are a comma separated list of names or a single regular expression. "
              "Using regular expressions does not automatically discover Kafka topics created after the processor started.")
          .withDefaultValue(magic_enum::enum_name(consume_kafka::TopicNameFormatEnum::Names))
          .withAllowedValues(magic_enum::enum_names<consume_kafka::TopicNameFormatEnum>())
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto HonorTransactions =
      core::PropertyDefinitionBuilder<>::createProperty("Honor Transactions")
          .withDescription(
              "Specifies whether or not MiNiFi should honor transactional guarantees when communicating with Kafka. If false, the Processor will use "
              "an \"isolation level\" of "
              "read_uncomitted. This means that messages will be received as soon as they are written to Kafka but will be pulled, even if the "
              "producer cancels the transactions. "
              "If this value is true, MiNiFi will not receive any messages for which the producer's transaction was canceled, but this can result in "
              "some latency since the consumer "
              "must wait for the producer to finish its entire transaction instead of pulling as the messages become available.")
          .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
          .withDefaultValue("true")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto GroupID =
      core::PropertyDefinitionBuilder<>::createProperty("Group ID")
          .withDescription(
              "A Group ID is used to identify consumers that are within the same consumer group. Corresponds to Kafka's 'group.id' property.")
          .supportsExpressionLanguage(true)
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto OffsetReset =
      core::PropertyDefinitionBuilder<3>::createProperty("Offset Reset")
          .withDescription(
              "Allows you to manage the condition when there is no initial offset in Kafka or if the current offset does not exist any more on the "
              "server (e.g. because that "
              "data has been deleted). Corresponds to Kafka's 'auto.offset.reset' property.")
          .withDefaultValue(magic_enum::enum_name(consume_kafka::OffsetResetPolicyEnum::latest))
          .withAllowedValues(magic_enum::enum_names<consume_kafka::OffsetResetPolicyEnum>())
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto KeyAttributeEncoding =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<utils::KafkaEncoding>()>::createProperty("Key Attribute Encoding")
          .withDescription(
              "FlowFiles that are emitted have an attribute named 'kafka.key'. This property dictates how the value of the attribute should be "
              "encoded.")
          .withDefaultValue(magic_enum::enum_name(utils::KafkaEncoding::UTF8))
          .withAllowedValues(magic_enum::enum_names<utils::KafkaEncoding>())
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto MessageDemarcator =
      core::PropertyDefinitionBuilder<>::createProperty("Message Demarcator")
          .withDescription(
              "Since KafkaConsumer receives messages in batches, you have an option to output FlowFiles which contains all Kafka messages in a "
              "single batch "
              "for a given topic and partition and this property allows you to provide a string (interpreted as UTF-8) to use for demarcating apart "
              "multiple Kafka messages. "
              "This is an optional property and if not provided each Kafka message received will result in a single FlowFile which time it is "
              "triggered. ")
          .supportsExpressionLanguage(true)
          .build();
  EXTENSIONAPI static constexpr auto MessageHeaderEncoding =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<utils::KafkaEncoding>()>::createProperty("Message Header Encoding")
          .withDescription(
              "Any message header that is found on a Kafka message will be added to the outbound FlowFile as an attribute. This property indicates "
              "the Character Encoding "
              "to use for deserializing the headers.")
          .withDefaultValue(magic_enum::enum_name(utils::KafkaEncoding::UTF8))
          .withAllowedValues(magic_enum::enum_names<utils::KafkaEncoding>())
          .build();
  EXTENSIONAPI static constexpr auto HeadersToAddAsAttributes =
      core::PropertyDefinitionBuilder<>::createProperty("Headers To Add As Attributes")
          .withDescription(
              "A comma separated list to match against all message headers. Any message header whose name matches an item from the list will be "
              "added to the FlowFile "
              "as an Attribute. If not specified, no Header values will be added as FlowFile attributes. The behaviour on when multiple headers of "
              "the same name are present is set using "
              "the Duplicate Header Handling attribute.")
          .build();
  EXTENSIONAPI static constexpr auto DuplicateHeaderHandling =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<consume_kafka::MessageHeaderPolicyEnum>()>::createProperty("Duplicate Header Handling")
          .withDescription(
              "For headers to be added as attributes, this option specifies how to handle cases where multiple headers are present with the same "
              "key. "
              "For example in case of receiving these two headers: \"Accept: text/html\" and \"Accept: application/xml\" and we want to attach the "
              "value of \"Accept\" "
              "as a FlowFile attribute:\n"
              " - \"Keep First\" attaches: \"Accept -> text/html\"\n"
              " - \"Keep Latest\" attaches: \"Accept -> application/xml\"\n"
              " - \"Comma-separated Merge\" attaches: \"Accept -> text/html, application/xml\"\n")
          .withDefaultValue(magic_enum::enum_name(consume_kafka::MessageHeaderPolicyEnum::KEEP_LATEST))
          .withAllowedValues(magic_enum::enum_names<consume_kafka::MessageHeaderPolicyEnum>())
          .build();
  EXTENSIONAPI static constexpr auto MaxPollRecords =
      core::PropertyDefinitionBuilder<>::createProperty("Max Poll Records")
          .withDescription("Specifies the maximum number of records Kafka should return when polling each time the processor is triggered.")
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .withDefaultValue(DEFAULT_MAX_POLL_RECORDS)
          .build();
  EXTENSIONAPI static constexpr auto MaxPollTime =
      core::PropertyDefinitionBuilder<>::createProperty("Max Poll Time")
          .withDescription(
              "Specifies the maximum amount of time the consumer can use for polling data from the brokers. "
              "Polling is a blocking operation, so the upper limit of this value is specified in 4 seconds.")
          .withValidator(consume_kafka::CONSUME_KAFKA_MAX_POLL_TIME_TYPE)
          .withDefaultValue(DEFAULT_MAX_POLL_TIME)
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto SessionTimeout =
      core::PropertyDefinitionBuilder<>::createProperty("Session Timeout")
          .withDescription(
              "Client group session and failure detection timeout. The consumer sends periodic heartbeats "
              "to indicate its liveness to the broker. If no hearts are received by the broker for a group member within "
              "the session timeout, the broker will remove the consumer from the group and trigger a rebalance. "
              "The allowed range is configured with the broker configuration properties group.min.session.timeout.ms and "
              "group.max.session.timeout.ms.")
          .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
          .withDefaultValue("60 seconds")
          .build();

  EXTENSIONAPI static constexpr auto CommitPolicy =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<consume_kafka::CommitPolicyEnum>()>::createProperty("Commit Offsets Policy")
          .withDescription(
              "NoCommit disables offset commiting entirely. "
              "AutoCommit configures Kafka to automatically increase offsets after serving the messages. "
              "CommitAfterBatch commits offsets after the messages has been converted to flowfiles. "
              "CommitFromIncomingFlowFiles consumes incoming flowfiles and commits the offsets based on their attributes. ")
          .withDefaultValue(magic_enum::enum_name(consume_kafka::CommitPolicyEnum::CommitAfterBatch))
          .withAllowedValues(magic_enum::enum_names<consume_kafka::CommitPolicyEnum>())
          .build();

  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(KafkaProcessorBase::Properties,
      std::to_array<core::PropertyReference>({KafkaBrokers,
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
          SessionTimeout,
          CommitPolicy}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success",
      "Incoming Kafka messages as flowfiles. Depending on the demarcation strategy, this can be one or multiple messages per flowfile."};
  EXTENSIONAPI static constexpr auto Commited = core::RelationshipDefinition{"commited",
      "Only when using \"Commit from incoming flowfiles\" policy. Flowfiles that were used for commiting offsets are routed here."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure",
      "Only when using \"Commit from incoming flowfiles\" policy. Flowfiles that were malformed for commiting offsets are routed here."};

  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  EXTENSIONAPI static constexpr auto KafkaTopicAttribute = core::OutputAttributeDefinition<>{
      "kafka.topic", {Success}, "The topic the message or message bundle is from"};
  EXTENSIONAPI static constexpr auto KafkaPartitionAttribute = core::OutputAttributeDefinition<>{
      "kafka.partition", {Success}, "The partition of the topic the message or message bundle is from"};
  EXTENSIONAPI static constexpr auto KafkaCountAttribute = core::OutputAttributeDefinition<>{
      "kafka.count", {Success}, "The number of messages written if more than one"};
  EXTENSIONAPI static constexpr auto KafkaKeyAttribute = core::OutputAttributeDefinition<>{
      "kafka.key", {Success}, "The key of the message if present and if single message"};
  EXTENSIONAPI static constexpr auto KafkaOffsetAttribute = core::OutputAttributeDefinition<>{
      "kafka.offset", {Success}, "The offset of the message (or largest offset of the message bundle)"};

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit ConsumeKafka(std::string_view name, const utils::Identifier& uuid = utils::Identifier())
      : KafkaProcessorBase(name, uuid, core::logging::LoggerFactory<ConsumeKafka>::getLogger(uuid)) {}

  ConsumeKafka(const ConsumeKafka&) = delete;
  ConsumeKafka(ConsumeKafka&&) = delete;
  ConsumeKafka& operator=(const ConsumeKafka&) = delete;
  ConsumeKafka& operator=(ConsumeKafka&&) = delete;
  ~ConsumeKafka() override = default;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  struct KafkaMessageLocation {
    std::string topic;
    int32_t partition;
    auto operator<=>(const KafkaMessageLocation&) const = default;
  };

  class MessageBundle {
   public:
    MessageBundle() = default;
    void pushBack(utils::rd_kafka_message_unique_ptr message) {
      largest_offset_ = std::max(largest_offset_, message->offset);
      messages_.push_back(std::move(message));
    }
    [[nodiscard]] int64_t getLargestOffset() const { return largest_offset_; }

    [[nodiscard]] const std::vector<utils::rd_kafka_message_unique_ptr>& getMessages() const { return messages_; }

   private:
    std::vector<utils::rd_kafka_message_unique_ptr> messages_;
    int64_t largest_offset_ = 0;
  };
  friend struct ::std::hash<KafkaMessageLocation>;

  void createTopicPartitionList();
  void extendConfigFromDynamicProperties(const core::ProcessContext& context) const;
  void configureNewConnection(core::ProcessContext& context);
  static std::string extractMessage(const rd_kafka_message_t& rkmessage);
  std::unordered_map<KafkaMessageLocation, MessageBundle> pollKafkaMessages();
  std::string resolve_duplicate_headers(const std::vector<std::string>& matching_headers) const;
  std::vector<std::string> get_matching_headers(const rd_kafka_message_t& message, const std::string& header_name) const;
  std::vector<std::pair<std::string, std::string>> getFlowFilesAttributesFromMessageHeaders(const rd_kafka_message_t& message) const;
  void addAttributesToSingleMessageFlowFile(core::FlowFile& flow_file, const rd_kafka_message_t& message) const;
  void addAttributesToMessageBundleFlowFile(core::FlowFile& flow_file, const MessageBundle& message_bundle) const;
  void processMessages(core::ProcessSession& session, const std::unordered_map<KafkaMessageLocation, MessageBundle>& message_bundles) const;
  void processMessageBundles(core::ProcessSession& session, const std::unordered_map<KafkaMessageLocation, MessageBundle>& message_bundles,
      std::string_view message_demarcator) const;

  void commitOffsetsFromMessages(const std::unordered_map<KafkaMessageLocation, MessageBundle>& message_bundles) const;
  void commitOffsetsFromIncomingFlowFiles(core::ProcessSession& session) const;

  std::vector<std::string> topic_names_{};
  consume_kafka::TopicNameFormatEnum topic_name_format_ = consume_kafka::TopicNameFormatEnum::Names;

  utils::KafkaEncoding key_attribute_encoding_ = utils::KafkaEncoding::UTF8;
  utils::KafkaEncoding message_header_encoding_ = utils::KafkaEncoding::UTF8;

  std::optional<std::string> message_demarcator_;

  consume_kafka::MessageHeaderPolicyEnum duplicate_header_handling_ = consume_kafka::MessageHeaderPolicyEnum::KEEP_LATEST;
  std::optional<std::vector<std::string>> headers_to_add_as_attributes_;

  uint32_t max_poll_records_{};
  std::chrono::milliseconds max_poll_time_milliseconds_{};
  consume_kafka::CommitPolicyEnum commit_policy_ = consume_kafka::CommitPolicyEnum::CommitAfterBatch;

  std::unique_ptr<rd_kafka_t, utils::rd_kafka_consumer_deleter> consumer_;
  std::unique_ptr<rd_kafka_conf_t, utils::rd_kafka_conf_deleter> conf_;
  std::unique_ptr<rd_kafka_topic_partition_list_t, utils::rd_kafka_topic_partition_list_deleter> kf_topic_partition_list_;
};

}  // namespace org::apache::nifi::minifi::processors
