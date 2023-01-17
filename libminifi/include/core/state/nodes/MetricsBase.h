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
#include <unordered_map>

#include "../Value.h"
#include "../PublishedMetricProvider.h"
#include "core/Core.h"
#include "core/Connectable.h"

namespace org::apache::nifi::minifi::state::response {

class ResponseNode;
using SharedResponseNode = gsl::not_null<std::shared_ptr<ResponseNode>>;

/**
 * Purpose: Defines a metric. Serialization is intended to be thread safe.
 */
class ResponseNode : public core::Connectable, public PublishedMetricProvider {
 public:
  ResponseNode()
      : core::Connectable("metric"),
        is_array_(false) {
  }

  explicit ResponseNode(std::string name)
      : core::Connectable(std::move(name)),
        is_array_(false) {
  }

  ResponseNode(std::string name, const utils::Identifier& uuid)
      : core::Connectable(std::move(name), uuid),
        is_array_(false) {
  }

  ~ResponseNode() override = default;

  static std::vector<SerializedResponseNode> serializeAndMergeResponseNodes(const std::vector<SharedResponseNode>& nodes);

  virtual std::vector<SerializedResponseNode> serialize() = 0;

  void yield() override {
  }

  bool isRunning() const override {
    return true;
  }

  bool isWorkAvailable() override {
    return true;
  }

  bool isArray() const {
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
  DeviceInformation(std::string name, const utils::Identifier& uuid)
      : ResponseNode(std::move(name), uuid) {
  }

  explicit DeviceInformation(std::string name)
      : ResponseNode(std::move(name)) {
  }
};

/**
 * Purpose: Defines a metric that
 */
class ObjectNode : public ResponseNode {
 public:
  explicit ObjectNode(std::string name, const utils::Identifier& uuid = {})
      : ResponseNode(std::move(name), uuid) {
  }

  void add_node(const SharedResponseNode &node) {
    nodes_[node->getName()].push_back(node);
  }

  std::string getName() const override {
    return Connectable::getName();
  }

  std::vector<SerializedResponseNode> serialize() override;

  bool isEmpty() override {
    return nodes_.empty();
  }

 protected:
  std::unordered_map<std::string, std::vector<SharedResponseNode>> nodes_;
};

/**
 * Purpose: Retrieves Metrics from the defined class. The current Metric, which is a consumable for any reader of Metrics must have the ability to set metrics.
 *
 */
class ResponseNodeSource {
 public:
  virtual ~ResponseNodeSource() = default;
  virtual SharedResponseNode getResponseNode() = 0;
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
