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

#include <memory>
#include <vector>
#include <mutex>

#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "core/state/MetricsPublisher.h"
#include "core/state/nodes/MetricsBase.h"
#include "utils/LogUtils.h"
#include "utils/StoppableThread.h"

namespace org::apache::nifi::minifi::state {

class LogMetricsPublisher : public MetricsPublisherImpl {
 public:
  using MetricsPublisherImpl::MetricsPublisherImpl;

  MINIFIAPI static constexpr const char* Description = "Serializes all the configured metrics into a json output and writes the json to the MiNiFi logs periodically";

  void initialize(const std::shared_ptr<Configure>& configuration, const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader) override;
  void clearMetricNodes() override;
  void loadMetricNodes() override;
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
