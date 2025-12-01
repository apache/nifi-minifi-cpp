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

#include <deque>
#include <mutex>
#include <unordered_set>
#include <regex>
#include <utility>
#include <string>
#include <memory>

#include "core/logging/LoggerProperties.h"
#include "utils/ThreadPool.h"
#include "utils/StagingQueue.h"
#include "utils/RegexUtils.h"
#include "properties/Configure.h"
#include "spdlog/sinks/base_sink.h"
#include "minifi-cpp/controllers/SSLContextServiceInterface.h"
#include "minifi-cpp/utils/TimeUtil.h"

namespace org::apache::nifi::minifi::controllers {
class SSLContextServiceInterface;
}  // namespace org::apache::nifi::minifi::controllers

namespace org::apache::nifi::minifi::core::logging {

class AlertSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  AlertSink(const AlertSink&) = delete;
  AlertSink(AlertSink&&) = delete;
  AlertSink& operator=(const AlertSink&&) = delete;
  AlertSink& operator=(AlertSink&&) = delete;

  static std::shared_ptr<AlertSink> create(const std::string& prop_name_prefix, const std::shared_ptr<LoggerProperties>& logger_properties, std::shared_ptr<Logger> logger);

  void initialize(std::shared_ptr<AgentIdentificationProvider> agent_id, std::shared_ptr<controllers::SSLContextServiceInterface> ssl_service);

  ~AlertSink() override;

 private:
  struct Config {
    std::string url;
    int batch_size;
    std::chrono::milliseconds flush_period;
    std::chrono::milliseconds rate_limit;
    int buffer_limit;
    utils::Regex filter;
    spdlog::level::level_enum level;
  };

  struct Services {
    std::shared_ptr<controllers::SSLContextServiceInterface> ssl_service;
    std::shared_ptr<AgentIdentificationProvider> agent_id;
  };

  class LogBuffer {
    friend class AlertSink;
   public:
    static LogBuffer allocate(size_t size);
    LogBuffer commit();
    [[nodiscard]]
    size_t size() const;
   private:
    size_t size_{0};
    std::deque<std::pair<std::string, size_t>> data_;
  };

  class LiveLogSet {
    using Hash = size_t;
    const std::chrono::milliseconds lifetime_{};
    std::unordered_set<Hash> hashes_to_ignore_;
    std::deque<std::pair<std::chrono::milliseconds, Hash>> timestamped_hashes_;
   public:
    explicit LiveLogSet(std::chrono::milliseconds lifetime): lifetime_(lifetime) {}

    bool tryAdd(std::chrono::milliseconds now, Hash hash);
  };

  AlertSink(Config config, std::shared_ptr<Logger> logger);

  void run();
  void send(Services& services);

  void sink_it_(const spdlog::details::log_msg& msg) override;
  void flush_() override;

  Config config_;
  LiveLogSet live_logs_;

  std::atomic_bool running_{true};
  std::mutex mtx_;
  std::shared_ptr<utils::timeutils::Clock> clock_ = utils::timeutils::getClock();
  std::chrono::milliseconds next_flush_;
  std::condition_variable cv_;
  std::thread flush_thread_;

  utils::StagingQueue<LogBuffer> buffer_;

  std::atomic<gsl::owner<Services*>> services_{nullptr};

  std::shared_ptr<Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::logging
