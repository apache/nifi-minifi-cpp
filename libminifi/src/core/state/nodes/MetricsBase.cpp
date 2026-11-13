// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
