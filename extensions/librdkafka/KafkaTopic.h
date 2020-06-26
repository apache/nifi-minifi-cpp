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

#ifndef EXTENSIONS_LIBRDKAFKA_KAFKATOPIC_H_
#define EXTENSIONS_LIBRDKAFKA_KAFKATOPIC_H_

#include "rdkafka.h"

#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class KafkaTopic {
 public:
  explicit KafkaTopic(gsl::owner<rd_kafka_topic_t*> topic_reference)
      : topic_reference_(topic_reference) {
  }

  ~KafkaTopic() {
    if (topic_reference_) {
      rd_kafka_topic_destroy(topic_reference_);
    }
  }

  rd_kafka_topic_t *getTopic() const {
    return topic_reference_;
  }

 private:
  gsl::owner<rd_kafka_topic_t*> topic_reference_;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_LIBRDKAFKA_KAFKATOPIC_H_
