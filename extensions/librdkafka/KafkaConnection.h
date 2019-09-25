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

#ifndef NIFI_MINIFI_CPP_KAFKACONNECTION_H
#define NIFI_MINIFI_CPP_KAFKACONNECTION_H

#include <atomic>
#include <mutex>
#include <string>
#include "core/logging/LoggerConfiguration.h"
#include "core/logging/Logger.h"
#include "rdkafka.h"
#include "KafkaTopic.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class KafkaConnectionKey {
  public:
    std::string client_id_;
    std::string brokers_;

    bool operator <(const KafkaConnectionKey& rhs) const {
      return std::tie(brokers_, client_id_) < std::tie(rhs.brokers_, rhs.client_id_);
    }
};

class KafkaConnection {
 public:

  explicit KafkaConnection(const KafkaConnectionKey &key);

  ~KafkaConnection();

  void remove();

  void removeConnection();

  bool initialized() const;

  void setConnection(rd_kafka_t *producer);

  rd_kafka_t *getConnection() const;

  bool hasTopic(const std::string &topic) const;

  std::shared_ptr<KafkaTopic> getTopic(const std::string &topic) const;

  KafkaConnectionKey const * const getKey() const;

  void putTopic(const std::string &topicName, const std::shared_ptr<KafkaTopic> &topic);

  static void logCallback(const rd_kafka_t* rk, int level, const char* /*fac*/, const char* buf);

  bool tryUse();

  friend class KafkaLease;

 private:

  std::shared_ptr<logging::Logger> logger_;

  std::mutex lease_mutex_;

  bool lease_;

  bool initialized_;

  KafkaConnectionKey key_;

  std::map<std::string, std::shared_ptr<KafkaTopic>> topics_;

  rd_kafka_t *kafka_connection_;

  std::atomic<bool> poll_;
  std::thread thread_kafka_poll_;

  static void modifyLoggers(const std::function<void(std::unordered_map<const rd_kafka_t*, std::weak_ptr<logging::Logger>>&)>& func) {
    static std::mutex loggers_mutex;
    static std::unordered_map<const rd_kafka_t*, std::weak_ptr<logging::Logger>> loggers;

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

class KafkaLease {
  public:
    ~KafkaLease() {
      std::lock_guard<std::mutex> lock(conn_->lease_mutex_);
      conn_->lease_ = false;
    }
    std::shared_ptr<KafkaConnection> getConn() const {
      return conn_;
    }
    friend class KafkaPool;
  private:
    KafkaLease(std::shared_ptr<KafkaConnection> conn) // This one should be private, and only KafkaPool can call (friend).
            : conn_(conn) {
    }

    std::shared_ptr<KafkaConnection> conn_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_KAFKACONNECTION_H
