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

