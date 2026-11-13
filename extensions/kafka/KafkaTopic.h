// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "rdkafka_utils.h"

namespace org::apache::nifi::minifi::processors {

class KafkaTopic {
 public:
  explicit KafkaTopic(utils::rd_kafka_topic_unique_ptr&& topic_reference) : topic_reference_(std::move(topic_reference)) {}

  KafkaTopic(const KafkaTopic&) = delete;
  KafkaTopic& operator=(const KafkaTopic&) = delete;
  KafkaTopic(KafkaTopic&&) = delete;
  KafkaTopic& operator=(KafkaTopic&&) = delete;

  ~KafkaTopic() = default;

  [[nodiscard]] rd_kafka_topic_t* getTopic() const { return topic_reference_.get(); }

 private:
  utils::rd_kafka_topic_unique_ptr topic_reference_;
};

}  // namespace org::apache::nifi::minifi::processors
