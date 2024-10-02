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

#include "core/state/Value.h"
#include "core/state/PublishedMetricProvider.h"
#include "core/Core.h"
#include "core/Connectable.h"
#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "core/state/nodes/ResponseNode.h"

namespace org::apache::nifi::minifi::state::response {

/**
 * Purpose: Defines a metric that
 */
class DeviceInformation : public ResponseNodeImpl {
 public:
  DeviceInformation(std::string_view name, const utils::Identifier& uuid)
      : ResponseNodeImpl(name, uuid) {
  }

  explicit DeviceInformation(std::string_view name)
      : ResponseNodeImpl(name) {
  }
};

/**
 * Purpose: Defines a metric that
 */
class ObjectNode : public ResponseNodeImpl {
 public:
  explicit ObjectNode(const std::string_view name, const utils::Identifier& uuid = {})
      : ResponseNodeImpl(name, uuid) {
  }

  void add_node(const SharedResponseNode &node) {
    nodes_[node->getName()].push_back(node);
  }

  std::string getName() const override {
    return ConnectableImpl::getName();
  }

  std::vector<SerializedResponseNode> serialize() override;

  bool isEmpty() override {
    return nodes_.empty();
  }

 protected:
  std::unordered_map<std::string, std::vector<SharedResponseNode>> nodes_;
};

}  // namespace org::apache::nifi::minifi::state::response
