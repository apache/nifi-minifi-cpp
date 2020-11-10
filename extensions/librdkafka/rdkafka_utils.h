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

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/logging/LoggerConfiguration.h"
#include "utils/OptionalUtils.h"
#include "rdkafka.h"

// TODO(hunyadi): check if these would be useful in PublishKafka

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

enum class KafkaEncoding {
  UTF8,
  HEX
};

struct rd_kafka_conf_deleter {
  void operator()(rd_kafka_conf_t* ptr) const noexcept { rd_kafka_conf_destroy(ptr); }
};

struct rd_kafka_producer_deleter {
  void operator()(rd_kafka_t* ptr) const noexcept {
    std::cerr << "\u001b[37;1mproducer_deleter\u001b[0m" << std::endl;
    rd_kafka_resp_err_t flush_ret = rd_kafka_flush(ptr, 10000 /* ms */);
    if (RD_KAFKA_RESP_ERR__TIMED_OUT == flush_ret) {
      std::cerr << "Deleting producer failed: time-out while trying to flush" << std::endl;
    }
    rd_kafka_destroy(ptr);
    std::cerr << "\u001b[37;1mproducer_deleter done\u001b[0m" << std::endl;
  }
};

struct rd_kafka_consumer_deleter {
  void operator()(rd_kafka_t* ptr) const noexcept {
    std::cerr << "\u001b[37;1mconsumer_deleter\u001b[0m" << std::endl;
    // rd_kafka_unsubscribe(ptr);
    rd_kafka_consumer_close(ptr);
    std::cerr << "\u001b[37;1m***\u001b[0m" << std::endl;
    rd_kafka_destroy(ptr);
    std::cerr << "\u001b[37;1mconsumer_deleter done\u001b[0m" << std::endl;
  }
};

struct rd_kafka_topic_partition_list_deleter {
  void operator()(rd_kafka_topic_partition_list_t* ptr) const noexcept { rd_kafka_topic_partition_list_destroy(ptr); }
};

struct rd_kafka_topic_conf_deleter {
  void operator()(rd_kafka_topic_conf_t* ptr) const noexcept { rd_kafka_topic_conf_destroy(ptr); }
};
struct rd_kafka_topic_deleter {
  void operator()(rd_kafka_topic_t* ptr) const noexcept { std::cerr << "\u001b[37;1mtopic_deleter\u001b[0m" << std::endl; rd_kafka_topic_destroy(ptr); }
};

struct rd_kafka_message_deleter {
  void operator()(rd_kafka_message_t* ptr) const noexcept { rd_kafka_message_destroy(ptr); }
};

struct rd_kafka_headers_deleter {
  void operator()(rd_kafka_headers_t* ptr) const noexcept { rd_kafka_headers_destroy(ptr); }
};

template <typename T>
void kafka_headers_for_each(const rd_kafka_headers_t* headers, T&& key_value_handle) {
  const char *key;  // Null terminated, not to be freed
  const void *value;
  std::size_t size;
  for (std::size_t i = 0; RD_KAFKA_RESP_ERR_NO_ERROR == rd_kafka_header_get_all(headers, i, &key, &value, &size); ++i) {
    std::forward<T>(key_value_handle)(std::string(key), std::string(static_cast<const char*>(value), size));
  }
}

void setKafkaConfigurationField(rd_kafka_conf_t* configuration, const std::string& field_name, const std::string& value);
void print_kafka_message(const rd_kafka_message_t* rkmessage, const std::shared_ptr<logging::Logger>& logger);
std::string get_encoded_string(const std::string& input, KafkaEncoding encoding);
optional<std::string> get_encoded_message_key(const rd_kafka_message_t* message, KafkaEncoding encoding);

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
