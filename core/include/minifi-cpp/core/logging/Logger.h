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

enum LOG_LEVEL {
  trace = 0,
  debug = 1,
  info = 2,
  warn = 3,
  err = 4,
  critical = 5,
  off = 6
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

class BaseLogger {
 public:
  virtual ~BaseLogger();

  virtual void log_string(LOG_LEVEL level, std::string str) = 0;
  virtual bool should_log(LOG_LEVEL level) = 0;
  [[nodiscard]] virtual LOG_LEVEL level() const = 0;
};

inline constexpr auto map_args = utils::overloaded {
    [](auto&& f) requires(std::is_invocable_v<decltype(f)>) { return std::invoke(std::forward<decltype(f)>(f)); },
    [](auto&& value) { return std::forward<decltype(value)>(value); }
};

template<typename... Args>
using log_format_string = fmt::format_string<std::invoke_result_t<decltype(map_args), Args>...>;

class Logger : public BaseLogger {
 public:
  Logger(Logger const&) = delete;
  Logger& operator=(Logger const&) = delete;

  template<typename ...Args>
  void log_with_level(LOG_LEVEL log_level, log_format_string<Args...> fmt, Args&& ...args) {
    return log(mapToSpdLogLevel(log_level), std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_critical(log_format_string<Args...> fmt, Args&& ...args) {
    log(spdlog::level::critical, std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_error(log_format_string<Args...> fmt, Args&& ...args) {
    log(spdlog::level::err, std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_warn(log_format_string<Args...> fmt, Args&& ...args) {
    log(spdlog::level::warn, std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_info(log_format_string<Args...> fmt, Args&& ...args) {
    log(spdlog::level::info, std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_debug(log_format_string<Args...> fmt, Args&& ...args) {
    log(spdlog::level::debug, std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_trace(log_format_string<Args...> fmt, Args&& ...args) {
    log(spdlog::level::trace, std::move(fmt), std::forward<Args>(args)...);
  }

  void set_max_log_size(int size) {
    max_log_size_ = size;
  }

  bool should_log(LOG_LEVEL level) override;
  void log_string(LOG_LEVEL level, std::string str) override;
  LOG_LEVEL level() const override;

  virtual std::optional<std::string> get_id() = 0;

 protected:
  Logger(std::shared_ptr<spdlog::logger> delegate, std::shared_ptr<LoggerControl> controller);

  Logger(std::shared_ptr<spdlog::logger> delegate); // NOLINT

  std::shared_ptr<spdlog::logger> delegate_;
  std::shared_ptr<LoggerControl> controller_;

  std::mutex mutex_;

 private:
  std::string trimToMaxSizeAndAddId(std::string my_string) {
    auto max_log_size = max_log_size_.load();
    if (max_log_size >= 0 && my_string.size() > gsl::narrow<size_t>(max_log_size))
      my_string = my_string.substr(0, max_log_size);
    if (auto id = get_id()) {
      my_string += *id;
    }
    return my_string;
  }

  template<typename ...Args>
  std::string stringify(fmt::format_string<Args...> fmt, Args&&... args) {
    auto log_message = fmt::format(std::move(fmt), std::forward<Args>(args)...);
    return trimToMaxSizeAndAddId(std::move(log_message));
  }

  template<typename ...Args>
  inline void log(spdlog::level::level_enum level, log_format_string<Args...> fmt, Args&& ...args) {
    if (controller_ && !controller_->is_enabled())
      return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!delegate_->should_log(level)) {
      return;
    }
    delegate_->log(level, stringify(std::move(fmt), map_args(std::forward<Args>(args))...));
  }

  std::atomic<int> max_log_size_{LOG_BUFFER_SIZE};
};

}  // namespace org::apache::nifi::minifi::core::logging
