// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
#include "FlowStatusBuilder.h"

namespace org::apache::nifi::minifi::c2 {

class ControllerSocketMetricsPublisher : public state::MetricsPublisherImpl, public ControllerSocketReporter {
 public:
  using MetricsPublisherImpl::MetricsPublisherImpl;
  MINIFIAPI static constexpr const char* Description = "Provides the response nodes for c2 operations through localized environment through a simple TCP socket.";

  void clearMetricNodes() override;
  void loadMetricNodes(core::ProcessGroup* root) override;

  std::unordered_map<std::string, QueueSize> getQueueSizes() override;
  std::unordered_set<std::string> getFullConnections() override;
  std::unordered_set<std::string> getConnections() override;
  std::string getAgentManifest() override;

  void setRoot(core::ProcessGroup* root) override;
  void setFlowStatusDependencies(core::BulletinStore* bulletin_store, const std::filesystem::path& flowfile_repo_dir, const std::filesystem::path& content_repo_dir) override;
  std::string getFlowStatus(const std::vector<FlowStatusRequest>& requests) override;

 protected:
  c2::HeartbeatJsonSerializer heartbeat_json_serializer_;
  std::mutex queue_metrics_node_mutex_;
  std::shared_ptr<state::response::ResponseNode> queue_metrics_node_;
  FlowStatusBuilder flow_status_builder_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ControllerSocketMetricsPublisher>::getLogger();
};

}  // namespace org::apache::nifi::minifi::c2
