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

#include <string>
#include <vector>
#include <memory>

#include "core/Processor.h"
#include "core/logging/LoggerConfiguration.h"
#include "rdkafka.h"
#include "rdkafka_utils.h"
#include "KafkaConnection.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class ConsumeKafka : public core::Processor {
 public:
  static constexpr char const* ProcessorName = "ConsumeKafka";

  // Supported Properties
  static core::Property KafkaBrokers;
  static core::Property SecurityProtocol;
  static core::Property TopicNames;
  static core::Property TopicNameFormat;
  static core::Property HonorTransactions;
  static core::Property GroupID;
  static core::Property OffsetReset;
  static core::Property KeyAttributeEncoding;
  static core::Property MessageDemarcator;
  static core::Property MessageHeaderEncoding;
  static core::Property HeadersToAddAsAttributes;
  static core::Property MaxPollRecords;
  static core::Property MaxUncommittedTime;
  static core::Property CommunicationsTimeout;

  // Supported Relationships
  static const core::Relationship Success;

  // Security Protocol allowable values
  static constexpr char const* SECURITY_PROTOCOL_PLAINTEXT = "PLAINTEXT";
  static constexpr char const* SECURITY_PROTOCOL_SSL = "SSL";
  static constexpr char const* SECURITY_PROTOCOL_SASL_PLAINTEXT = "SASL_PLAINTEXT";
  static constexpr char const* SECURITY_PROTOCOL_SASL_SSL = "SASL_SSL";

  // Topic Name Format allowable values
  static constexpr char const* TOPIC_FORMAT_NAMES = "Names";
  static constexpr char const* TOPIC_FORMAT_PATTERNS = "Patterns";

  // Offset Reset allowable values
  static constexpr char const* OFFSET_RESET_EARLIEST = "earliest";
  static constexpr char const* OFFSET_RESET_LATEST = "latest";
  static constexpr char const* OFFSET_RESET_NONE = "none";

  // Key Attribute Encoding allowable values
  static constexpr char const* KEY_ATTR_ENCODING_UTF_8 = "UTF-8";
  static constexpr char const* KEY_ATTR_ENCODING_HEX = "Hex";

  // Message Header Encoding allowable values
  static constexpr char const* MSG_HEADER_ENCODING_UTF_8 = "UTF-8";

  // Flowfile attributes written
  static constexpr char const* KAFKA_MESSAGE_KEY_ATTR = "kafka.key";

  explicit ConsumeKafka(std::string name, utils::Identifier uuid = utils::Identifier()) :
      Processor(name, uuid),
      logger_(logging::LoggerFactory<ConsumeKafka>::getLogger()) {}

  // virtual ~ConsumeKafka() = default;
  virtual ~ConsumeKafka() {
    logger_->log_debug("\u001b[37;1mConsumeKafka destructor.\u001b[0m");
  }

 public:
  bool supportsDynamicProperties() override {
    return true;
  }
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /* sessionFactory */) override;
  /**
   * Execution trigger for the RetryFlowFile Processor
   * @param context processor context
   * @param session processor session reference.
   */
  void onTrigger(core::ProcessContext* context, core::ProcessSession* session) override;

  // Initialize, overwrite by NiFi RetryFlowFile
  void initialize() override;

 private:
  // void rebalance_cb(rd_kafka_t* rk, rd_kafka_resp_err_t err, rd_kafka_topic_partition_list_t* partitions, void* /*opaque*/);

  void createTopicPartitionList();
  void configureNewConnection(const core::ProcessContext* context);
  std::string extract_message(const rd_kafka_message_t* rkmessage);
  utils::KafkaEncoding key_attr_encoding_attr_to_enum();

 private:
  std::vector<std::string> kafka_brokers_;
  std::string security_protocol_;
  std::vector<std::string> topic_names_;
  std::string topic_name_format_;  // Easier handled as string than enum
  bool honor_transactions_;
  std::string group_id_;
  std::string offset_reset_;  // Easier handled as string than enum
  std::string key_attribute_encoding_;  // Easier handled as string than enum
  std::string message_demarcator_;
  std::string message_header_encoding_;  // This is a placeholder, only UTF-8 is supported here
  std::vector<std::string> headers_to_add_as_attributes_;
  utils::optional<unsigned int> max_poll_records_;
  utils::optional<unsigned int> max_uncommitted_time_seconds_;
  std::chrono::milliseconds communications_timeout_milliseconds_;

  std::unique_ptr<rd_kafka_t, utils::rd_kafka_consumer_deleter> consumer_;
  std::unique_ptr<rd_kafka_conf_t, utils::rd_kafka_conf_deleter> conf_;
  std::unique_ptr<rd_kafka_topic_partition_list_t, utils::rd_kafka_topic_partition_list_deleter> kf_topic_partition_list_;

  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<ConsumeKafka>::getLogger()};
};

REGISTER_RESOURCE(ConsumeKafka, "Consumes messages from Apache Kafka and transform them into MiNiFi FlowFiles."); // NOLINT

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
