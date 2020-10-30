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

// TODO(hunyadi): check if these would be useful in PublishKafka

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

void setKafkaConfigurationField(rd_kafka_conf_t* configuration, const std::string& field_name, const std::string& value) {
  static std::array<char, 512U> errstr{};
  rd_kafka_conf_res_t result;
  result = rd_kafka_conf_set(configuration, field_name.c_str(), value.c_str(), errstr.data(), errstr.size());
  // logger_->log_debug("Setting kafka configuration field bootstrap.servers:= %s", value);
  if (RD_KAFKA_CONF_OK != result) {
    const std::string error_msg { errstr.begin(), errstr.end() };
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "rd_kafka configuration error" + error_msg);
  }
}

void print_kafka_message(const rd_kafka_message_t* rkmessage, const std::shared_ptr<logging::Logger>& logger) {
  if (RD_KAFKA_RESP_ERR_NO_ERROR != rkmessage->err) {
    const std::string error_msg = "ConsumeKafka: received error message from broker. Librdkafka error msg: " + std::string(rd_kafka_err2str(rkmessage->err));
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, error_msg);
  }
  std::string topicName = rd_kafka_topic_name(rkmessage->rkt);
  std::string message(reinterpret_cast<char*>(rkmessage->payload), rkmessage->len);
  const char* key = reinterpret_cast<const char*>(rkmessage->key);
  const std::size_t key_len = rkmessage->key_len;
  rd_kafka_timestamp_type_t tstype;
  int64_t timestamp;
  rd_kafka_headers_t *hdrs;
  timestamp = rd_kafka_message_timestamp(rkmessage, &tstype);
  const char *tsname = "?";
  if (tstype != RD_KAFKA_TIMESTAMP_NOT_AVAILABLE) {
    if (tstype == RD_KAFKA_TIMESTAMP_CREATE_TIME) {
      tsname = "create time";
    } else if (tstype == RD_KAFKA_TIMESTAMP_LOG_APPEND_TIME) {
      tsname = "log append time";
    }
  }

  logger->log_debug("Message: \u001b[33m%s\u001b[0m", message.c_str());
  logger->log_debug("Topic: %s, Key: %.*s,\n\u001b[32mOffset: %" PRId64 ", (%zd bytes)\nMessage timestamp: %s %" PRId64 " \u001b[33m(%ds ago)\u001b[0m", topicName.c_str(),
      key != nullptr ? key_len : 6, ((key != nullptr ? key : "[None]")), rkmessage->offset, rkmessage->len, tsname,
      timestamp, !timestamp ? 0 : static_cast<int>(time(NULL)) - static_cast<int>(timestamp / 1000));
}

optional<std::string> get_encoded_message_key(const rd_kafka_message_t* message, KafkaEncoding encoding) {
  if (nullptr == message->key) {
    return {};
  }
  const char* key = reinterpret_cast<const char*>(message->key);
  const std::size_t key_len = message->key_len;
  switch (encoding) {
    case KafkaEncoding::UTF8:
      return std::string{key, key_len};
    case KafkaEncoding::HEX:
      return StringUtils::to_hex({key, key_len}, /* uppercase = */ true);
    default:
      throw std::runtime_error("Invalid encoding selected for kafka message key encoding.");
  }
  return {};
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
