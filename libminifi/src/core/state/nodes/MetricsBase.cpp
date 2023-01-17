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
#include "core/state/nodes/MetricsBase.h"

namespace org::apache::nifi::minifi::state::response {

std::vector<SerializedResponseNode> ResponseNode::serializeAndMergeResponseNodes(const std::vector<SharedResponseNode>& nodes) {
  if (nodes.empty()) {
    return {};
  }

  std::vector<SerializedResponseNode> result;
  for (const auto& node: nodes) {
    auto serialized = node->serialize();
    result.insert(result.end(), serialized.begin(), serialized.end());
  }
  return result;
}

std::vector<SerializedResponseNode> ObjectNode::serialize() {
  std::vector<SerializedResponseNode> serialized;
  for (const auto& [name, nodes] : nodes_) {
    if (nodes.empty()) {
      continue;
    }
    SerializedResponseNode inner_node;
    inner_node.name = nodes[0]->getName();
    for (auto &embed : ResponseNode::serializeAndMergeResponseNodes(nodes)) {
      if (!embed.empty() || embed.keep_empty) {
        inner_node.children.push_back(std::move(embed));
      }
    }
    serialized.push_back(std::move(inner_node));
  }
  return serialized;
}

}  // namespace org::apache::nifi::minifi::state::response
