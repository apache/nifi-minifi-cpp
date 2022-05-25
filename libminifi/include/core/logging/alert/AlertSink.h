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

#include "spdlog/sinks/base_sink.h"
#include "core/logging/LoggerProperties.h"
#include "utils/ThreadPool.h"
#include "controllers/SSLContextService.h"
#include "core/controller/ControllerServiceProvider.h"
#include "properties/Configure.h"

#include <regex>
#include <unordered_set>
#include <deque>

namespace org::apache::nifi::minifi::core::logging {

class AlertSink : public spdlog::sinks::base_sink<std::mutex> {
  struct Config {
    std::string url;
    std::optional<std::string> ssl_service_name;
    int batch_size;
    std::chrono::milliseconds flush_period;
    std::chrono::milliseconds rate_limit;
    int buffer_limit;
    std::regex filter;
  };
 public:
  explicit AlertSink(const std::shared_ptr<LoggerProperties>& logger_properties, std::shared_ptr<Logger> logger);
  void initialize(core::controller::ControllerServiceProvider* controller, std::shared_ptr<Configure> agent_config);

 private:
  utils::TaskRescheduleInfo run();

  void sink_it_(const spdlog::details::log_msg& msg) override;
  void flush_() override;

  std::optional<Config> config_;
  std::unordered_set<size_t> ignored_hashes_;
  std::deque<std::pair<std::chrono::steady_clock::time_point, size_t>> ordered_hashes_;
  std::mutex log_mtx_;
  int buffer_size_{0};
  std::deque<std::pair<std::string, size_t>> buffer_;

  utils::ThreadPool<utils::TaskRescheduleInfo> thread_pool_{1};
  utils::Identifier task_id_;

  std::shared_ptr<controllers::SSLContextService> ssl_service_;

  std::shared_ptr<Configure> agent_config_;
  std::shared_ptr<Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::logging
