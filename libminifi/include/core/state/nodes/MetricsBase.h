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
#ifndef LIBMINIFI_INCLUDE_METRICS_METRICSBASE_H_
#define LIBMINIFI_INCLUDE_METRICS_METRICSBASE_H_

#include <vector>
#include <memory>
#include <string>

#include "../Value.h"
#include "core/Core.h"
#include "core/Connectable.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

/**
 * Purpose: Defines a metric. Serialization is intended to be thread safe.
 */
class ResponseNode : public core::Connectable {
 public:
  ResponseNode()
      : core::Connectable("metric"),
        is_array_(false) {
  }

  ResponseNode(std::string name)
      : core::Connectable(name),
        is_array_(false) {
  }

  ResponseNode(std::string name, utils::Identifier & uuid)
      : core::Connectable(name, uuid),
        is_array_(false) {
  }
  virtual ~ResponseNode() {

  }

  virtual std::vector<SerializedResponseNode> serialize() = 0;

  virtual void yield() {
  }
  virtual bool isRunning() {
    return true;
  }
  virtual bool isWorkAvailable() {
    return true;
  }

  bool isArray() {
    return is_array_;
  }

  virtual bool isEmpty() {
    return false;
  }

 protected:

  bool is_array_;

  void setArray(bool array) {
    is_array_ = array;
  }

};

/**
 * Purpose: Defines a metric that
 */
class DeviceInformation : public ResponseNode {
 public:
  DeviceInformation(std::string name, utils::Identifier & uuid)
      : ResponseNode(name, uuid) {
  }
  DeviceInformation(std::string name)
      : ResponseNode(name) {
  }
};

/**
 * Purpose: Defines a metric that
 */
class ObjectNode : public ResponseNode {
 public:
  ObjectNode(std::string name, utils::Identifier uuid = utils::Identifier())
      : ResponseNode(name, uuid) {
  }

  void add_node(const std::shared_ptr<ResponseNode> &node) {
    nodes_.push_back(node);
  }

  virtual std::string getName() const {
    return Connectable::getName();
  }

  virtual std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;
//    SerializedResponseNode outer_node;
    //  outer_node.name = getName();
    for (auto &node : nodes_) {
      SerializedResponseNode inner_node;
      inner_node.name = node->getName();
      for (auto &embed : node->serialize()) {
        inner_node.children.push_back(std::move(embed));
      }
      serialized.push_back(std::move(inner_node));
    }
    //serialized.push_back(std::move(outer_node));
    return serialized;
  }

  virtual bool isEmpty() {
    return nodes_.empty();
  }

 protected:
  std::vector<std::shared_ptr<ResponseNode>> nodes_;

};

/**
 * Purpose: Retrieves Metrics from the defined class. The current Metric, which is a consumable for any reader of Metrics must have the ability to set metrics.
 *
 */
class ResponseNodeSource {
 public:

  ResponseNodeSource() {

  }

  virtual ~ResponseNodeSource() {
  }

  /**
   * Retrieves all metrics from this source.
   * @param metric_vector -- metrics will be placed in this vector.
   * @return result of the get operation.
   *  0 Success
   *  1 No error condition, but cannot obtain lock in timely manner.
   *  -1 failure
   */
  virtual int16_t getResponseNodes(std::vector<std::shared_ptr<ResponseNode>> &metric_vector) = 0;

  virtual int16_t getMetricNodes(std::vector<std::shared_ptr<ResponseNode>> &metric_vector) = 0;

};

/**
 * Purpose: Retrieves Metrics from the defined class. The current Metric, which is a consumable for any reader of Metrics must have the ability to set metrics.
 *
 */
class MetricsNodeSource : public ResponseNodeSource {
 public:

  MetricsNodeSource() {

  }

  virtual ~MetricsNodeSource() {
  }

  /**
   * Retrieves all metrics from this source.
   * @param metric_vector -- metrics will be placed in this vector.
   * @return result of the get operation.
   *  0 Success
   *  1 No error condition, but cannot obtain lock in timely manner.
   *  -1 failure
   */
  virtual int16_t getResponseNodes(std::vector<std::shared_ptr<ResponseNode>> &metric_vector) {
    return getMetricNodes(metric_vector);
  }

  virtual int16_t getMetricNodes(std::vector<std::shared_ptr<ResponseNode>> &metric_vector) = 0;

};

class NodeReporter {
 public:

  NodeReporter() {

  }

  virtual ~NodeReporter() {
  }

  /**
   * Retrieves all root response nodes from this source.
   * @param metric_vector -- metrics will be placed in this vector.
   * @return result of the get operation.
   *  0 Success
   *  1 No error condition, but cannot obtain lock in timely manner.
   *  -1 failure
   */
  virtual int16_t getResponseNodes(std::vector<std::shared_ptr<ResponseNode>> &metric_vector, uint16_t metricsClass) = 0;

  /**
   * Retrieves all metrics from this source.
   * @param metric_vector -- metrics will be placed in this vector.
   * @return result of the get operation.
   *  0 Success
   *  1 No error condition, but cannot obtain lock in timely manner.
   *  -1 failure
   */
  virtual int16_t getMetricsNodes(std::vector<std::shared_ptr<ResponseNode>> &metric_vector, uint16_t metricsClass) = 0;

};

/**
 * Purpose: Sink interface for all metrics. The current Metric, which is a consumable for any reader of Metrics must have the ability to set metrics.
 *
 */
class ResponseNodeSink {
 public:

  virtual ~ResponseNodeSink() {
  }
  /**
   * Setter for nodes in this sink.
   * @param metrics metrics to insert into the current sink.
   * @return result of the set operation.
   *  0 Success
   *  1 No error condition, but cannot obtain lock in timely manner.
   *  -1 failure
   */
  virtual int16_t setResponseNodes(const std::shared_ptr<ResponseNode> &metrics) = 0;

  /**
   * Setter for metrics nodes in this sink.
   * @param metrics metrics to insert into the current sink.
   * @return result of the set operation.
   *  0 Success
   *  1 No error condition, but cannot obtain lock in timely manner.
   *  -1 failure
   */
//  virtual int16_t setMetricsNodes(const std::shared_ptr<ResponseNode> &metrics) = 0;
};

} /* namespace metrics */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_METRICS_METRICSBASE_H_ */
