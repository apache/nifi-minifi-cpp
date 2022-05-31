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
#pragma once

#include <utility>
#include <vector>
#include <memory>
#include <string>
#include <optional>

#include "../Value.h"
#include "../PublishedMetricProvider.h"
#include "core/Core.h"
#include "core/Connectable.h"

namespace org::apache::nifi::minifi::state::response {

/**
 * Purpose: Defines a metric. Serialization is intended to be thread safe.
 */
class ResponseNode : public core::Connectable, public PublishedMetricProvider {
 public:
  ResponseNode()
      : core::Connectable("metric"),
        is_array_(false) {
  }

  ResponseNode(const std::string& name) // NOLINT
      : core::Connectable(name),
        is_array_(false) {
  }

  ResponseNode(const std::string& name, const utils::Identifier& uuid)
      : core::Connectable(name, uuid),
        is_array_(false) {
  }
  virtual ~ResponseNode() = default;

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
  DeviceInformation(const std::string& name, const utils::Identifier& uuid)
      : ResponseNode(name, uuid) {
  }
  DeviceInformation(const std::string& name) // NOLINT
      : ResponseNode(name) {
  }
};

/**
 * Purpose: Defines a metric that
 */
class ObjectNode : public ResponseNode {
 public:
  ObjectNode(const std::string& name, const utils::Identifier& uuid = {}) // NOLINT
      : ResponseNode(name, uuid) {
  }

  void add_node(const std::shared_ptr<ResponseNode> &node) {
    nodes_.push_back(node);
  }

  const std::vector<std::shared_ptr<ResponseNode>>& get_child_nodes() const {
    return nodes_;
  }

  std::string getName() const override {
    return Connectable::getName();
  }

  std::vector<SerializedResponseNode> serialize() override {
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
    // serialized.push_back(std::move(outer_node));
    return serialized;
  }

  bool isEmpty() override {
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
  ResponseNodeSource() = default;

  virtual ~ResponseNodeSource() = default;

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
  MetricsNodeSource() = default;

  virtual ~MetricsNodeSource() = default;

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
  struct ReportedNode {
    std::string name;
    bool is_array;
    std::vector<SerializedResponseNode> serialized_nodes;
  };

  NodeReporter() = default;

  virtual ~NodeReporter() = default;

  /**
   * Retrieves metrics node
   * @return metrics response node
   */
  virtual std::optional<ReportedNode> getMetricsNode(const std::string& metricsClass) const = 0;

  /**
   * Retrieves root nodes configured to be included in heartbeat
   * @param includeManifest -- determines if manifest is to be included
   * @return a list of response nodes
   */
  virtual std::vector<ReportedNode> getHeartbeatNodes(bool includeManifest) const = 0;

  /**
   * Retrieves the agent manifest to be sent as a response to C2 DESCRIBE manifest
   * @return the agent manifest response node
   */
  virtual ReportedNode getAgentManifest() = 0;
};

/**
 * Purpose: Sink interface for all metrics. The current Metric, which is a consumable for any reader of Metrics must have the ability to set metrics.
 *
 */
class ResponseNodeSink {
 public:
  virtual ~ResponseNodeSink() = default;
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
  virtual int16_t setMetricsNodes(const std::shared_ptr<ResponseNode> &metrics) = 0;
};

}  // namespace org::apache::nifi::minifi::state::response
