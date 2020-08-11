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

#include "KafkaConnection.h"

#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

KafkaConnection::KafkaConnection(const KafkaConnectionKey &key)
    : logger_(logging::LoggerFactory<KafkaConnection>::getLogger()),
      kafka_connection_(nullptr),
      poll_(false) {
  initialized_ = false;
  key_ = key;
}

KafkaConnection::~KafkaConnection() {
  remove();
}

void KafkaConnection::remove() {
  topics_.clear();
  removeConnection();
}

void KafkaConnection::removeConnection() {
  logger_->log_trace("KafkaConnection::removeConnection START: Client = %s -- Broker = %s", key_.client_id_, key_.brokers_);
  stopPoll();
  if (kafka_connection_) {
    rd_kafka_flush(kafka_connection_, 10 * 1000); /* wait for max 10 seconds */
    rd_kafka_destroy(kafka_connection_);
    modifyLoggers([&](std::unordered_map<const rd_kafka_t*, std::weak_ptr<logging::Logger>>& loggers) {
      loggers.erase(kafka_connection_);
    });
    kafka_connection_ = nullptr;
  }
  initialized_ = false;
  logger_->log_trace("KafkaConnection::removeConnection FINISH: Client = %s -- Broker = %s", key_.client_id_, key_.brokers_);
}

bool KafkaConnection::initialized() const {
  return initialized_;
}

void KafkaConnection::setConnection(gsl::owner<rd_kafka_t*> producer) {
  removeConnection();
  kafka_connection_ = producer;  // kafka_connection_ takes ownership from producer
  initialized_ = true;
  modifyLoggers([&](std::unordered_map<const rd_kafka_t*, std::weak_ptr<logging::Logger>>& loggers) {
    loggers[producer] = logger_;
  });
  startPoll();
}

rd_kafka_t *KafkaConnection::getConnection() const {
  return kafka_connection_;
}

bool KafkaConnection::hasTopic(const std::string &topic) const {
  return topics_.count(topic);
}

std::shared_ptr<KafkaTopic> KafkaConnection::getTopic(const std::string &topic) const {
  auto topicObj = topics_.find(topic);
  if (topicObj != topics_.end()) {
    return topicObj->second;
  }
  return nullptr;
}

KafkaConnectionKey const* KafkaConnection::getKey() const {
  return &key_;
}

void KafkaConnection::putTopic(const std::string &topicName, const std::shared_ptr<KafkaTopic> &topic) {
  topics_[topicName] = topic;
}

void KafkaConnection::logCallback(const rd_kafka_t* rk, int level, const char* /*fac*/, const char* buf) {
  std::shared_ptr<logging::Logger> logger;
  try {
    modifyLoggers([&](std::unordered_map<const rd_kafka_t*, std::weak_ptr<logging::Logger>>& loggers) {
      logger = loggers.at(rk).lock();
    });
  } catch (...) {
  }

  if (!logger) {
    return;
  }

  switch (level) {
    case 0:  // LOG_EMERG
    case 1:  // LOG_ALERT
    case 2:  // LOG_CRIT
    case 3:  // LOG_ERR
      logging::LOG_ERROR(logger) << buf;
      break;
    case 4:  // LOG_WARNING
      logging::LOG_WARN(logger) << buf;
      break;
    case 5:  // LOG_NOTICE
    case 6:  // LOG_INFO
      logging::LOG_INFO(logger) << buf;
      break;
    case 7:  // LOG_DEBUG
      logging::LOG_DEBUG(logger) << buf;
      break;
  }
}

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
