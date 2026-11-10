// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "KafkaConnection.h"

#include <memory>
#include <utility>

#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

KafkaConnection::KafkaConnection(KafkaConnectionKey key)
    : initialized_(false),
      key_(std::move(key)),
      poll_(false) {}

KafkaConnection::~KafkaConnection() {
  remove();
}

void KafkaConnection::remove() {
  topics_.clear();
  removeConnection();
}

void KafkaConnection::removeConnection() {
  stopPoll();
  if (kafka_connection_) {
    rd_kafka_flush(kafka_connection_, 10 * 1000); /* wait for max 10 seconds */
    rd_kafka_destroy(kafka_connection_);
    kafka_connection_ = nullptr;
  }
  initialized_ = false;
}

bool KafkaConnection::initialized() const {
  return initialized_;
}

void KafkaConnection::setConnection(utils::rd_kafka_producer_unique_ptr producer) {
  removeConnection();
  kafka_connection_ = gsl::owner<rd_kafka_t*>{producer.release()};  // kafka_connection_ takes ownership from producer
  initialized_ = true;
  startPoll();
}

rd_kafka_t* KafkaConnection::getConnection() const {
  return static_cast<rd_kafka_t*>(kafka_connection_);
}

bool KafkaConnection::hasTopic(const std::string& topic) const {
  return topics_.contains(topic);
}

std::shared_ptr<KafkaTopic> KafkaConnection::getTopic(const std::string& topic) const {
  if (const auto topicObj = topics_.find(topic); topicObj != topics_.end()) { return topicObj->second; }
  return nullptr;
}

KafkaConnectionKey const* KafkaConnection::getKey() const {
  return &key_;
}

void KafkaConnection::putTopic(const std::string& topicName, const std::shared_ptr<KafkaTopic>& topic) {
  topics_[topicName] = topic;
}

}  // namespace org::apache::nifi::minifi::processors
