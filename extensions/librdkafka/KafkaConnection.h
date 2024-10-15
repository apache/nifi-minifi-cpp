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

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "core/logging/LoggerFactory.h"
#include "core/logging/Logger.h"
#include "rdkafka.h"
#include "KafkaTopic.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

struct KafkaConnectionKey {
  std::string client_id_;
  std::string brokers_;

  bool operator< (const KafkaConnectionKey& rhs) const { return std::tie(brokers_, client_id_) <  std::tie(rhs.brokers_, rhs.client_id_); }
  bool operator<=(const KafkaConnectionKey& rhs) const { return std::tie(brokers_, client_id_) <= std::tie(rhs.brokers_, rhs.client_id_); }
  bool operator==(const KafkaConnectionKey& rhs) const { return std::tie(brokers_, client_id_) == std::tie(rhs.brokers_, rhs.client_id_); }
  bool operator!=(const KafkaConnectionKey& rhs) const { return std::tie(brokers_, client_id_) != std::tie(rhs.brokers_, rhs.client_id_); }
  bool operator> (const KafkaConnectionKey& rhs) const { return std::tie(brokers_, client_id_) >  std::tie(rhs.brokers_, rhs.client_id_); }
  bool operator>=(const KafkaConnectionKey& rhs) const { return std::tie(brokers_, client_id_) >= std::tie(rhs.brokers_, rhs.client_id_); }
};

class KafkaConnection {
 public:
  explicit KafkaConnection(KafkaConnectionKey key);

  KafkaConnection(const KafkaConnection&) = delete;
  KafkaConnection& operator=(KafkaConnection) = delete;

  ~KafkaConnection();

  void remove();

  void removeConnection();

  bool initialized() const;

  void setConnection(gsl::owner<rd_kafka_t*> producer);

  rd_kafka_t* getConnection() const;

  bool hasTopic(const std::string &topic) const;

  std::shared_ptr<KafkaTopic> getTopic(const std::string &topic) const;

  KafkaConnectionKey const* getKey() const;

  void putTopic(const std::string &topicName, const std::shared_ptr<KafkaTopic> &topic);

  static void logCallback(const rd_kafka_t* rk, int level, const char* /*fac*/, const char* buf);

 private:
  std::shared_ptr<core::logging::Logger> logger_;

  bool initialized_;

  KafkaConnectionKey key_;

  std::map<std::string, std::shared_ptr<KafkaTopic>> topics_;

  gsl::owner<rd_kafka_t*> kafka_connection_;

  std::atomic<bool> poll_;
  std::thread thread_kafka_poll_;

  static void modifyLoggers(const std::function<void(std::unordered_map<const rd_kafka_t*, std::weak_ptr<core::logging::Logger>>&)>& func) {
    static std::mutex loggers_mutex;
    static std::unordered_map<const rd_kafka_t*, std::weak_ptr<core::logging::Logger>> loggers;

    std::lock_guard<std::mutex> lock(loggers_mutex);
    func(loggers);
  }

  void stopPoll() {
    poll_ = false;
    logger_->log_debug("Stop polling");
    if (thread_kafka_poll_.joinable()) {
      thread_kafka_poll_.join();
    }
  }

  void startPoll() {
    poll_ = true;
    logger_->log_debug("Start polling");
    thread_kafka_poll_ = std::thread([this]{
        while (this->poll_) {
          rd_kafka_poll(this->kafka_connection_, 1000);
        }
    });
  }
};

}  // namespace org::apache::nifi::minifi::processors
