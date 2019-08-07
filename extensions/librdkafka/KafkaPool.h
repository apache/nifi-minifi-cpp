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

#ifndef NIFI_MINIFI_CPP_KAFKAPOOL_H
#define NIFI_MINIFI_CPP_KAFKAPOOL_H

#include "KafkaConnection.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class KafkaPool {
 public:

  explicit KafkaPool(int max)
      : max_(max) {
  }

  bool removeConnection(const KafkaConnectionKey &key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return map_.erase(key) == 1;
  }

  std::unique_ptr<KafkaLease> getOrCreateConnection(const KafkaConnectionKey &key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto connection = map_.find(key);
    std::shared_ptr<KafkaConnection> conn;
    if (connection == map_.end()) {
      // Not found, create new connection.
      conn = std::make_shared<KafkaConnection>(key);
      if (map_.size() == max_) {
        // Reached pool limit, remove the first one.
        map_.erase(map_.begin());
      }
      map_[key] = conn;
    } else {
      conn = connection->second;
    }
    std::unique_ptr<KafkaLease> lease;
    if (conn->tryUse()) {
      lease = std::unique_ptr<KafkaLease>(new KafkaLease(conn));
    }
    return lease;
  }

 private:
  std::mutex mutex_;

  size_t max_;

  std::map<KafkaConnectionKey, std::shared_ptr<KafkaConnection>> map_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_KAFKAPOOL_H
