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

#include <array>

#include "rdkafka_utils.h"

#include "Exception.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

void setKafkaConfigurationField(rd_kafka_conf_t& configuration, const std::string& field_name, const std::string& value) {
  static std::array<char, 512U> errstr{};
  rd_kafka_conf_res_t result;
  result = rd_kafka_conf_set(&configuration, field_name.c_str(), value.c_str(), errstr.data(), errstr.size());
  if (RD_KAFKA_CONF_OK != result) {
    const std::string error_msg { errstr.data() };
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "rd_kafka configuration error: " + error_msg);
  }
}

void print_topics_list(core::logging::Logger& logger, rd_kafka_topic_partition_list_t& kf_topic_partition_list) {
  for (int i = 0; i < kf_topic_partition_list.cnt; ++i) {
    logger.log_debug("kf_topic_partition_list: topic: %s, partition: %d, offset: %" PRId64 ".",
    kf_topic_partition_list.elems[i].topic, kf_topic_partition_list.elems[i].partition, kf_topic_partition_list.elems[i].offset);
  }
}

std::string get_human_readable_kafka_message_timestamp(const rd_kafka_message_t& rkmessage) {
  rd_kafka_timestamp_type_t tstype;
  int64_t timestamp;
  timestamp = rd_kafka_message_timestamp(&rkmessage, &tstype);
  const char *tsname = "?";
  if (tstype == RD_KAFKA_TIMESTAMP_CREATE_TIME) {
    tsname = "create time";
  } else if (tstype == RD_KAFKA_TIMESTAMP_LOG_APPEND_TIME) {
    tsname = "log append time";
  }
  const int64_t seconds_since_timestamp = timestamp == -1 ? 0 : static_cast<int64_t>(time(NULL)) - static_cast<int64_t>(timestamp / 1000);
  return {"[Timestamp](" + std::string(tsname) + " " + std::to_string(timestamp) + " (" + std::to_string(seconds_since_timestamp) + " s ago)"};
}

std::string get_human_readable_kafka_message_headers(const rd_kafka_message_t& rkmessage, core::logging::Logger& logger) {
  rd_kafka_headers_t* hdrs;
  const rd_kafka_resp_err_t get_header_response = rd_kafka_message_headers(&rkmessage, &hdrs);
  if (RD_KAFKA_RESP_ERR_NO_ERROR == get_header_response) {
    std::vector<std::string> header_list;
    kafka_headers_for_each(*hdrs, [&] (const std::string& key, gsl::span<const char> val) { header_list.emplace_back(key + ": " + std::string{ val.data(), val.size() }); });
    return StringUtils::join(", ", header_list);
  }
  if (RD_KAFKA_RESP_ERR__NOENT == get_header_response) {
    return "[None]";
  }
  logger.log_error("Failed to fetch message headers: %d: %s", rd_kafka_last_error(), rd_kafka_err2str(rd_kafka_last_error()));
  return "[Error]";
}

void print_kafka_message(const rd_kafka_message_t& rkmessage, core::logging::Logger& logger) {
  if (RD_KAFKA_RESP_ERR_NO_ERROR != rkmessage.err) {
    const std::string error_msg = "ConsumeKafka: received error message from broker. Librdkafka error msg: " + std::string(rd_kafka_err2str(rkmessage.err));
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, error_msg);
  }
  std::string topicName = rd_kafka_topic_name(rkmessage.rkt);
  std::string message(reinterpret_cast<char*>(rkmessage.payload), rkmessage.len);
  const char* key = reinterpret_cast<const char*>(rkmessage.key);
  const std::size_t key_len = rkmessage.key_len;

  std::string message_as_string;
  message_as_string += "[Topic](" + topicName + "), ";
  message_as_string += "[Key](" + (key != nullptr ? std::string(key, key_len) : std::string("[None]")) + "), ";
  message_as_string += "[Offset](" +  std::to_string(rkmessage.offset) + "), ";
  message_as_string += "[Message Length](" + std::to_string(rkmessage.len) + "), ";
  message_as_string += get_human_readable_kafka_message_timestamp(rkmessage) + "), ";
  message_as_string += "[Headers](";
  message_as_string += get_human_readable_kafka_message_headers(rkmessage, logger) + ")";
  message_as_string += "[Payload](" + message + ")";

  logger.log_debug("Message: %s", message_as_string.c_str());
}

std::string get_encoded_string(const std::string& input, KafkaEncoding encoding) {
  switch (encoding) {
    case KafkaEncoding::UTF8:
      return input;
    case KafkaEncoding::HEX:
      return StringUtils::to_hex(input, /* uppercase = */ true);
  }
  throw std::runtime_error("Invalid encoding selected: " + input);
}

std::optional<std::string> get_encoded_message_key(const rd_kafka_message_t& message, KafkaEncoding encoding) {
  if (nullptr == message.key) {
    return {};
  }
  return get_encoded_string({reinterpret_cast<const char*>(message.key), message.key_len}, encoding);
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
