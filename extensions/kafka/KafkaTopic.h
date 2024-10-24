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
