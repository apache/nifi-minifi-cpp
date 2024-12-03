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
class ResponseNode : public virtual core::Connectable, public virtual PublishedMetricProvider {
 public:
  ~ResponseNode() override = default;

  static std::vector<SerializedResponseNode> serializeAndMergeResponseNodes(const std::vector<SharedResponseNode>& nodes);

  virtual std::vector<SerializedResponseNode> serialize() = 0;
  virtual bool isArray() const = 0;
  virtual bool isEmpty() = 0;
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

}  // namespace org::apache::nifi::minifi::state::response
