/**
 *
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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_METRICS_QUEUEMETRICS_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_METRICS_QUEUEMETRICS_H_

#include <sstream>
#include <map>

#include "MetricsBase.h"
#include "Connection.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace metrics {

/**
 * Justification and Purpose: Provides Connection queue metrics. Provides critical information to the
 * C2 server.
 *
 */
class QueueMetrics : public Metrics {
 public:

  QueueMetrics(const std::string &name, uuid_t uuid)
      : Metrics(name, uuid) {
  }

  QueueMetrics(const std::string &name)
      : Metrics(name, 0) {
  }

  QueueMetrics()
      : Metrics("QueueMetrics", 0) {
  }

  std::string getName() {
    return "QueueMetrics";
  }

  void addConnection(const std::shared_ptr<minifi::Connection> &connection) {
    if (nullptr != connection) {
      connections.insert(std::make_pair(connection->getName(), connection));
    }
  }

  std::vector<MetricResponse> serialize() {
    std::vector<MetricResponse> serialized;
    for (auto conn : connections) {
      auto connection = conn.second;
      MetricResponse parent;
      parent.name = connection->getName();
      MetricResponse datasize;
      datasize.name = "datasize";
      datasize.value = std::to_string(connection->getQueueDataSize());

      MetricResponse datasizemax;
      datasizemax.name = "datasizemax";
      datasizemax.value = std::to_string(connection->getMaxQueueDataSize());

      MetricResponse queuesize;
      queuesize.name = "queued";
      queuesize.value = std::to_string(connection->getQueueSize());

      MetricResponse queuesizemax;
      queuesizemax.name = "queuedmax";
      queuesizemax.value = std::to_string(connection->getMaxQueueSize());

      parent.children.push_back(datasize);
      parent.children.push_back(datasizemax);
      parent.children.push_back(queuesize);
      parent.children.push_back(queuesizemax);

      serialized.push_back(parent);
    }
    return serialized;
  }

 protected:
  std::map<std::string, std::shared_ptr<minifi::Connection>> connections;
};

} /* namespace metrics */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_STATE_METRICS_QUEUEMETRICS_H_ */
