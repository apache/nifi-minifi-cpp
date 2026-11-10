// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <vector>
#include <mutex>

#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "core/state/MetricsPublisher.h"
#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "utils/LogUtils.h"
#include "utils/StoppableThread.h"

namespace org::apache::nifi::minifi::state {

class LogMetricsPublisher : public MetricsPublisherImpl {
 public:
  using MetricsPublisherImpl::MetricsPublisherImpl;

  MINIFIAPI static constexpr const char* Description = "Serializes all the configured metrics into a json output and writes the json to the MiNiFi logs periodically";

  void initialize(const std::shared_ptr<Configure>& configuration, const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader) override;
  void clearMetricNodes() override;
  void loadMetricNodes(core::ProcessGroup* root) override;
  ~LogMetricsPublisher() override;

 private:
  void readLoggingInterval();
  void readLogLevel();
  void logMetrics();

  std::unique_ptr<utils::StoppableThread> metrics_logger_thread_;
  utils::LogUtils::LogLevelOption log_level_ = utils::LogUtils::LogLevelOption::LOGGING_INFO;
  std::chrono::milliseconds logging_interval_;
  std::mutex response_nodes_mutex_;
  std::vector<state::response::SharedResponseNode> response_nodes_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<LogMetricsPublisher>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::state
