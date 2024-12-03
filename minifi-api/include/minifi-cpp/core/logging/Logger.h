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

#include "utils/gsl.h"
#include "utils/Enum.h"
#include "utils/GeneralUtils.h"
#include "fmt/chrono.h"
#include "fmt/std.h"
#include "fmt/ostream.h"

namespace org::apache::nifi::minifi::core::logging {

enum LOG_LEVEL {
  trace = 0,
  debug = 1,
  info = 2,
  warn = 3,
  err = 4,
  critical = 5,
  off = 6
};

inline constexpr auto map_args = utils::overloaded {
    [](auto&& f) requires(std::is_invocable_v<decltype(f)>) { return std::invoke(std::forward<decltype(f)>(f)); },
    [](auto&& value) { return std::forward<decltype(value)>(value); }
};

template<typename... Args>
using log_format_string = fmt::format_string<std::invoke_result_t<decltype(map_args), Args>...>;

class Logger {
 public:
  template<typename ...Args>
  void log_with_level(LOG_LEVEL log_level, log_format_string<Args...> fmt, Args&& ...args) {
    return log(log_level, std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_critical(log_format_string<Args...> fmt, Args&& ...args) {
    log(LOG_LEVEL::critical, std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_error(log_format_string<Args...> fmt, Args&& ...args) {
    log(LOG_LEVEL::err, std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_warn(log_format_string<Args...> fmt, Args&& ...args) {
    log(LOG_LEVEL::warn, std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_info(log_format_string<Args...> fmt, Args&& ...args) {
    log(LOG_LEVEL::info, std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_debug(log_format_string<Args...> fmt, Args&& ...args) {
    log(LOG_LEVEL::debug, std::move(fmt), std::forward<Args>(args)...);
  }

  template<typename ...Args>
  void log_trace(log_format_string<Args...> fmt, Args&& ...args) {
    log(LOG_LEVEL::trace, std::move(fmt), std::forward<Args>(args)...);
  }

  virtual void set_max_log_size(int size) = 0;
  virtual std::optional<std::string> get_id() = 0;
  virtual void log_string(LOG_LEVEL level, std::string str) = 0;
  virtual bool should_log(LOG_LEVEL level) = 0;
  [[nodiscard]] virtual LOG_LEVEL level() const = 0;

  virtual ~Logger() = default;

 protected:
  virtual int getMaxLogSize() = 0;

 private:
  std::string trimToMaxSizeAndAddId(std::string my_string) {
    auto max_log_size = getMaxLogSize();
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
  inline void log(LOG_LEVEL level, log_format_string<Args...> fmt, Args&& ...args) {
    if (!should_log(level)) {
      return;
    }
    log_string(level, stringify(std::move(fmt), map_args(std::forward<Args>(args))...));
  }
};

}  // namespace org::apache::nifi::minifi::core::logging
