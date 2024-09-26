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
#include <utility>
#include <vector>

#include "KafkaProcessorBase.h"
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "io/StreamPipe.h"
#include "rdkafka.h"
#include "rdkafka_utils.h"
#include "KafkaConnection.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi {

namespace core {
class ConsumeKafkaMaxPollTimePropertyType : public TimePeriodPropertyType {
 public:
  constexpr ~ConsumeKafkaMaxPollTimePropertyType() override { }  // NOLINT see comment at grandparent

  [[nodiscard]] ValidationResult validate(const std::string& subject, const std::string& input) const override;
};

inline constexpr ConsumeKafkaMaxPollTimePropertyType CONSUME_KAFKA_MAX_POLL_TIME_TYPE{};
}  // namespace core

namespace processors {

class ConsumeKafka : public KafkaProcessorBase {
 public:
  // Security Protocol allowable values
  static constexpr std::string_view SECURITY_PROTOCOL_PLAINTEXT = "plaintext";
  static constexpr std::string_view SECURITY_PROTOCOL_SSL = "ssl";

  // Topic Name Format allowable values
  static constexpr std::string_view TOPIC_FORMAT_NAMES = "Names";
  static constexpr std::string_view TOPIC_FORMAT_PATTERNS = "Patterns";

  // Offset Reset allowable values
  static constexpr std::string_view OFFSET_RESET_EARLIEST = "earliest";
  static constexpr std::string_view OFFSET_RESET_LATEST = "latest";
  static constexpr std::string_view OFFSET_RESET_NONE = "none";

  // Key Attribute Encoding allowable values
  static constexpr std::string_view KEY_ATTR_ENCODING_UTF_8 = "UTF-8";
  static constexpr std::string_view KEY_ATTR_ENCODING_HEX = "Hex";

  // Message Header Encoding allowable values
  static constexpr std::string_view MSG_HEADER_ENCODING_UTF_8 = "UTF-8";
  static constexpr std::string_view MSG_HEADER_ENCODING_HEX = "Hex";

  // Duplicate Header Handling allowable values
  static constexpr std::string_view MSG_HEADER_KEEP_FIRST = "Keep First";
  static constexpr std::string_view MSG_HEADER_KEEP_LATEST = "Keep Latest";
  static constexpr std::string_view MSG_HEADER_COMMA_SEPARATED_MERGE = "Comma-separated Merge";

  // Flowfile attributes written
  static constexpr std::string_view KAFKA_COUNT_ATTR = "kafka.count";  // Always 1 until we start supporting merging from batches
  static constexpr std::string_view KAFKA_MESSAGE_KEY_ATTR = "kafka.key";
  static constexpr std::string_view KAFKA_OFFSET_ATTR = "kafka.offset";
  static constexpr std::string_view KAFKA_PARTITION_ATTR = "kafka.partition";
  static constexpr std::string_view KAFKA_TOPIC_ATTR = "kafka.topic";

  static constexpr std::string_view DEFAULT_MAX_POLL_RECORDS = "10000";
  static constexpr std::string_view DEFAULT_MAX_POLL_TIME = "4 seconds";

  static constexpr const std::size_t METADATA_COMMUNICATIONS_TIMEOUT_MS{ 60000 };

  EXTENSIONAPI static constexpr const char* Description = "Consumes messages from Apache Kafka and transform them into MiNiFi FlowFiles. "
      "The application should make sure that the processor is triggered at regular intervals, even if no messages are expected, "
      "to serve any queued callbacks waiting to be called. Rebalancing can also only happen on trigger.";

  EXTENSIONAPI static constexpr auto KafkaBrokers = core::PropertyDefinitionBuilder<>::createProperty("Kafka Brokers")
      .withDescription("A comma-separated list of known Kafka Brokers in the format <host>:<port>.")
      .withPropertyType(core::StandardPropertyTypes::NON_BLANK_TYPE)
      .withDefaultValue("localhost:9092")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto TopicNames = core::PropertyDefinitionBuilder<>::createProperty("Topic Names")
      .withDescription("The name of the Kafka Topic(s) to pull from. Multiple topic names are supported as a comma separated list.")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto TopicNameFormat = core::PropertyDefinitionBuilder<2>::createProperty("Topic Name Format")
      .withDescription("Specifies whether the Topic(s) provided are a comma separated list of names or a single regular expression. "
          "Using regular expressions does not automatically discover Kafka topics created after the processor started.")
      .withAllowedValues({TOPIC_FORMAT_NAMES, TOPIC_FORMAT_PATTERNS})
      .withDefaultValue(TOPIC_FORMAT_NAMES)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto HonorTransactions = core::PropertyDefinitionBuilder<>::createProperty("Honor Transactions")
      .withDescription(
          "Specifies whether or not MiNiFi should honor transactional guarantees when communicating with Kafka. If false, the Processor will use an \"isolation level\" of "
          "read_uncomitted. This means that messages will be received as soon as they are written to Kafka but will be pulled, even if the producer cancels the transactions. "
          "If this value is true, MiNiFi will not receive any messages for which the producer's transaction was canceled, but this can result in some latency since the consumer "
          "must wait for the producer to finish its entire transaction instead of pulling as the messages become available.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto GroupID = core::PropertyDefinitionBuilder<>::createProperty("Group ID")
      .withDescription("A Group ID is used to identify consumers that are within the same consumer group. Corresponds to Kafka's 'group.id' property.")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto OffsetReset = core::PropertyDefinitionBuilder<3>::createProperty("Offset Reset")
      .withDescription("Allows you to manage the condition when there is no initial offset in Kafka or if the current offset does not exist any more on the server (e.g. because that "
          "data has been deleted). Corresponds to Kafka's 'auto.offset.reset' property.")
      .withAllowedValues({OFFSET_RESET_EARLIEST, OFFSET_RESET_LATEST, OFFSET_RESET_NONE})
      .withDefaultValue(OFFSET_RESET_LATEST)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto KeyAttributeEncoding = core::PropertyDefinitionBuilder<2>::createProperty("Key Attribute Encoding")
      .withDescription("FlowFiles that are emitted have an attribute named 'kafka.key'. This property dictates how the value of the attribute should be encoded.")
      .withAllowedValues({KEY_ATTR_ENCODING_UTF_8, KEY_ATTR_ENCODING_HEX})
      .withDefaultValue(KEY_ATTR_ENCODING_UTF_8)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MessageDemarcator = core::PropertyDefinitionBuilder<>::createProperty("Message Demarcator")
      .withDescription("Since KafkaConsumer receives messages in batches, you have an option to output FlowFiles which contains all Kafka messages in a single batch "
          "for a given topic and partition and this property allows you to provide a string (interpreted as UTF-8) to use for demarcating apart multiple Kafka messages. "
          "This is an optional property and if not provided each Kafka message received will result in a single FlowFile which time it is triggered. ")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto MessageHeaderEncoding = core::PropertyDefinitionBuilder<2>::createProperty("Message Header Encoding")
      .withDescription("Any message header that is found on a Kafka message will be added to the outbound FlowFile as an attribute. This property indicates the Character Encoding "
          "to use for deserializing the headers.")
      .withAllowedValues({MSG_HEADER_ENCODING_UTF_8, MSG_HEADER_ENCODING_HEX})
      .withDefaultValue(MSG_HEADER_ENCODING_UTF_8)
      .build();
  EXTENSIONAPI static constexpr auto HeadersToAddAsAttributes = core::PropertyDefinitionBuilder<>::createProperty("Headers To Add As Attributes")
      .withDescription("A comma separated list to match against all message headers. Any message header whose name matches an item from the list will be added to the FlowFile "
          "as an Attribute. If not specified, no Header values will be added as FlowFile attributes. The behaviour on when multiple headers of the same name are present is set using "
          "the Duplicate Header Handling attribute.")
      .build();
  EXTENSIONAPI static constexpr auto DuplicateHeaderHandling = core::PropertyDefinitionBuilder<3>::createProperty("Duplicate Header Handling")
      .withDescription("For headers to be added as attributes, this option specifies how to handle cases where multiple headers are present with the same key. "
          "For example in case of receiving these two headers: \"Accept: text/html\" and \"Accept: application/xml\" and we want to attach the value of \"Accept\" "
          "as a FlowFile attribute:\n"
          " - \"Keep First\" attaches: \"Accept -> text/html\"\n"
          " - \"Keep Latest\" attaches: \"Accept -> application/xml\"\n"
          " - \"Comma-separated Merge\" attaches: \"Accept -> text/html, application/xml\"\n")
      .withAllowedValues({MSG_HEADER_KEEP_FIRST, MSG_HEADER_KEEP_LATEST, MSG_HEADER_COMMA_SEPARATED_MERGE})
      .withDefaultValue(MSG_HEADER_KEEP_LATEST)  // Mirroring NiFi behaviour
      .build();
  EXTENSIONAPI static constexpr auto MaxPollRecords = core::PropertyDefinitionBuilder<>::createProperty("Max Poll Records")
      .withDescription("Specifies the maximum number of records Kafka should return when polling each time the processor is triggered.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue(DEFAULT_MAX_POLL_RECORDS)
      .build();
  EXTENSIONAPI static constexpr auto MaxPollTime = core::PropertyDefinitionBuilder<>::createProperty("Max Poll Time")
      .withDescription("Specifies the maximum amount of time the consumer can use for polling data from the brokers. "
          "Polling is a blocking operation, so the upper limit of this value is specified in 4 seconds.")
      .withPropertyType(core::CONSUME_KAFKA_MAX_POLL_TIME_TYPE)
      .withDefaultValue(DEFAULT_MAX_POLL_TIME)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto SessionTimeout = core::PropertyDefinitionBuilder<>::createProperty("Session Timeout")
      .withDescription("Client group session and failure detection timeout. The consumer sends periodic heartbeats "
          "to indicate its liveness to the broker. If no hearts are received by the broker for a group member within "
          "the session timeout, the broker will remove the consumer from the group and trigger a rebalance. "
          "The allowed range is configured with the broker configuration properties group.min.session.timeout.ms and group.max.session.timeout.ms.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("60 seconds")
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(KafkaProcessorBase::Properties, std::to_array<core::PropertyReference>({
      KafkaBrokers,
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
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success",
      "Incoming Kafka messages as flowfiles. Depending on the demarcation strategy, this can be one or multiple flowfiles per message."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit ConsumeKafka(std::string_view name, const utils::Identifier& uuid = utils::Identifier()) :
      KafkaProcessorBase(name, uuid, core::logging::LoggerFactory<ConsumeKafka>::getLogger(uuid)) {}

  ~ConsumeKafka() override = default;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  void create_topic_partition_list();
  void extend_config_from_dynamic_properties(const core::ProcessContext& context);
  void configure_new_connection(core::ProcessContext& context);
  static std::string extract_message(const rd_kafka_message_t& rkmessage);
  std::vector<std::unique_ptr<rd_kafka_message_t, utils::rd_kafka_message_deleter>> poll_kafka_messages();
  utils::KafkaEncoding key_attr_encoding_attr_to_enum() const;
  utils::KafkaEncoding message_header_encoding_attr_to_enum() const;
  std::string resolve_duplicate_headers(const std::vector<std::string>& matching_headers) const;
  std::vector<std::string> get_matching_headers(const rd_kafka_message_t& message, const std::string& header_name) const;
  std::vector<std::pair<std::string, std::string>> get_flowfile_attributes_from_message_header(const rd_kafka_message_t& message) const;
  void add_kafka_attributes_to_flowfile(std::shared_ptr<core::FlowFile>& flow_file, const rd_kafka_message_t& message) const;
  std::optional<std::vector<std::shared_ptr<core::FlowFile>>> transform_pending_messages_into_flowfiles(core::ProcessSession& session) const;
  void process_pending_messages(core::ProcessSession& session);

  std::string kafka_brokers_;
  std::vector<std::string> topic_names_;
  std::string topic_name_format_;
  bool honor_transactions_{};
  std::string group_id_;
  std::string offset_reset_;
  std::string key_attribute_encoding_;
  std::string message_demarcator_;
  std::string message_header_encoding_;
  std::string duplicate_header_handling_;
  std::vector<std::string> headers_to_add_as_attributes_;
  std::size_t max_poll_records_{};
  std::chrono::milliseconds max_poll_time_milliseconds_{};
  std::chrono::milliseconds session_timeout_milliseconds_{};

  std::unique_ptr<rd_kafka_t, utils::rd_kafka_consumer_deleter> consumer_;
  std::unique_ptr<rd_kafka_conf_t, utils::rd_kafka_conf_deleter> conf_;
  std::unique_ptr<rd_kafka_topic_partition_list_t, utils::rd_kafka_topic_partition_list_deleter> kf_topic_partition_list_;

  // Intermediate container type for messages that have been processed, but are
  // not yet persisted (eg. in case of I/O error)
  std::vector<std::unique_ptr<rd_kafka_message_t, utils::rd_kafka_message_deleter>> pending_messages_;

  std::mutex do_not_call_on_trigger_concurrently_;
};

}  // namespace processors
}  // namespace org::apache::nifi::minifi
