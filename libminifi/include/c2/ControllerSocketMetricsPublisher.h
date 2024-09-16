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
#pragma once

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <memory>

#include "ControllerSocketReporter.h"
#include "core/state/MetricsPublisher.h"
#include "core/logging/LoggerFactory.h"
#include "c2/HeartbeatJsonSerializer.h"

namespace org::apache::nifi::minifi::c2 {

class ControllerSocketMetricsPublisher : public state::MetricsPublisherImpl, public ControllerSocketReporter {
 public:
  using MetricsPublisherImpl::MetricsPublisherImpl;
  MINIFIAPI static constexpr const char* Description = "Provides the response nodes for c2 operations through localized environment through a simple TCP socket.";

  void clearMetricNodes() override;
  void loadMetricNodes() override;

  std::unordered_map<std::string, QueueSize> getQueueSizes() override;
  std::unordered_set<std::string> getFullConnections() override;
  std::unordered_set<std::string> getConnections() override;
  std::string getAgentManifest() override;

 protected:
  c2::HeartbeatJsonSerializer heartbeat_json_serializer_;
  std::mutex queue_metrics_node_mutex_;
  std::shared_ptr<state::response::ResponseNode> queue_metrics_node_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ControllerSocketMetricsPublisher>::getLogger();
};

}  // namespace org::apache::nifi::minifi::c2
