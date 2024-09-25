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

#include <string>
#include <mutex>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>
#include <iostream>
#include <vector>
#include <algorithm>

#include "spdlog/common.h"
#include "spdlog/logger.h"
#include "utils/gsl.h"
#include "utils/Enum.h"
#include "utils/GeneralUtils.h"
#include "fmt/chrono.h"
#include "fmt/std.h"
#include "fmt/ostream.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::core::logging {

inline constexpr size_t LOG_BUFFER_SIZE = 4096;

class LoggerControl {
 public:
  LoggerControl();

  [[nodiscard]] bool is_enabled() const;

  void setEnabled(bool status);

 protected:
  std::atomic<bool> is_enabled_;
};

inline spdlog::level::level_enum mapToSpdLogLevel(LOG_LEVEL level) {
  switch (level) {
    case trace: return spdlog::level::trace;
    case debug: return spdlog::level::debug;
    case info: return spdlog::level::info;
    case warn: return spdlog::level::warn;
    case err: return spdlog::level::err;
    case critical: return spdlog::level::critical;
    case off: return spdlog::level::off;
  }
  throw std::invalid_argument(fmt::format("Invalid LOG_LEVEL {}", magic_enum::enum_underlying(level)));
}

inline LOG_LEVEL mapFromSpdLogLevel(spdlog::level::level_enum level) {
  switch (level) {
    case spdlog::level::trace: return LOG_LEVEL::trace;
    case spdlog::level::debug: return LOG_LEVEL::debug;
    case spdlog::level::info: return LOG_LEVEL::info;
    case spdlog::level::warn: return LOG_LEVEL::warn;
    case spdlog::level::err: return LOG_LEVEL::err;
    case spdlog::level::critical: return LOG_LEVEL::critical;
    case spdlog::level::off: return LOG_LEVEL::off;
    case spdlog::level::n_levels: break;
  }
  throw std::invalid_argument(fmt::format("Invalid spdlog::level::level_enum {}", magic_enum::enum_underlying(level)));
}

class LoggerBase : public Logger {
 public:
  LoggerBase(LoggerBase const&) = delete;
  LoggerBase& operator=(LoggerBase const&) = delete;

  void set_max_log_size(int size) override {
    max_log_size_ = size;
  }

  bool should_log(LOG_LEVEL level) override;
  void log_string(LOG_LEVEL level, std::string str) override;
  LOG_LEVEL level() const override;

 protected:
  LoggerBase(std::shared_ptr<spdlog::logger> delegate, std::shared_ptr<LoggerControl> controller);

  LoggerBase(std::shared_ptr<spdlog::logger> delegate); // NOLINT

  int getMaxLogSize() override {
    return max_log_size_.load();
  }

  std::shared_ptr<spdlog::logger> delegate_;
  std::shared_ptr<LoggerControl> controller_;

  std::mutex mutex_;

 private:
  std::atomic<int> max_log_size_{LOG_BUFFER_SIZE};
};

}  // namespace org::apache::nifi::minifi::core::logging
