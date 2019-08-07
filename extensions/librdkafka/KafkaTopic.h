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

#ifndef NIFI_MINIFI_CPP_KAFKATOPIC_H
#define NIFI_MINIFI_CPP_KAFKATOPIC_H

#include "rdkafka.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class KafkaTopic {
 public:
  KafkaTopic(rd_kafka_topic_t *topic_reference, rd_kafka_topic_conf_t *topic_conf)
      : topic_conf_(topic_conf),
        topic_reference_(topic_reference) {

  }

  ~KafkaTopic() {
    if (topic_reference_) {
      rd_kafka_topic_destroy(topic_reference_);
    }
    if (topic_conf_) {
      rd_kafka_topic_conf_destroy(topic_conf_);
    }
  }

  rd_kafka_topic_conf_t *getTopicConf() const {
    return topic_conf_;
  }

  rd_kafka_topic_t *getTopic() const {
    return topic_reference_;
  }

 private:
  rd_kafka_topic_conf_t *topic_conf_;
  rd_kafka_topic_t *topic_reference_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_KAFKATOPIC_H
