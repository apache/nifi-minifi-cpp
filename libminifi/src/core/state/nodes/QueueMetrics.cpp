// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/state/nodes/QueueMetrics.h"
#include "core/Resource.h"
#include "core/state/Value.h"

namespace org::apache::nifi::minifi::state::response {

std::vector<SerializedResponseNode> QueueMetrics::serialize() {
  std::vector<SerializedResponseNode> serialized;
  for (const auto& [_, connection] : connection_store_.getConnections()) {
    serialized.push_back({
      .name = connection->getName(),
      .children = {
        {.name = "datasize", .value = std::to_string(connection->getQueueDataSize())},
        {.name = "datasizemax", .value = std::to_string(connection->getBackpressureThresholdDataSize())},
        {.name = "queued", .value = std::to_string(connection->getQueueSize())},
        {.name = "queuedmax", .value = std::to_string(connection->getBackpressureThresholdCount())},
      }
    });
  }
  return serialized;
}

REGISTER_RESOURCE(QueueMetrics, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response

