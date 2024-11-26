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

#include <memory>
#include <utility>

#include "utils/Enum.h"

namespace org::apache::nifi::minifi::utils::LogUtils {

enum class LogLevelOption {
  LOGGING_TRACE,
  LOGGING_DEBUG,
  LOGGING_INFO,
  LOGGING_WARN,
  LOGGING_ERROR,
  LOGGING_CRITICAL,
  LOGGING_OFF
};

inline LogLevelOption mapToLogLevelOption(core::logging::LOG_LEVEL level) {
  switch (level) {
    case core::logging::trace: return LogLevelOption::LOGGING_TRACE;
    case core::logging::debug: return LogLevelOption::LOGGING_DEBUG;
    case core::logging::info: return LogLevelOption::LOGGING_INFO;
    case core::logging::warn: return LogLevelOption::LOGGING_WARN;
    case core::logging::err: return LogLevelOption::LOGGING_ERROR;
    case core::logging::critical: return LogLevelOption::LOGGING_CRITICAL;
    case core::logging::off: return LogLevelOption::LOGGING_OFF;
  }
  throw std::invalid_argument(fmt::format("Invalid LOG_LEVEL {}", magic_enum::enum_underlying(level)));
}

inline core::logging::LOG_LEVEL mapToLogLevel(LogLevelOption option) {
  switch (option) {
    case LogLevelOption::LOGGING_TRACE: return core::logging::trace;
    case LogLevelOption::LOGGING_DEBUG: return core::logging::debug;
    case LogLevelOption::LOGGING_INFO: return core::logging::info;
    case LogLevelOption::LOGGING_WARN: return core::logging::warn;
    case LogLevelOption::LOGGING_ERROR: return core::logging::err;
    case LogLevelOption::LOGGING_CRITICAL: return core::logging::critical;
    case LogLevelOption::LOGGING_OFF: return core::logging::off;
  }
  throw std::invalid_argument(fmt::format("Invalid LogLevelOption {}", magic_enum::enum_underlying(option)));
}

}  // namespace org::apache::nifi::minifi::utils::LogUtils

namespace magic_enum::customize {
using LogLevelOption = org::apache::nifi::minifi::utils::LogUtils::LogLevelOption;

template <>
constexpr customize_t enum_name<LogLevelOption>(LogLevelOption value) noexcept {
  switch (value) {
    case LogLevelOption::LOGGING_TRACE:
      return "TRACE";
    case LogLevelOption::LOGGING_DEBUG:
      return "DEBUG";
    case LogLevelOption::LOGGING_INFO:
      return "INFO";
    case LogLevelOption::LOGGING_WARN:
      return "WARN";
    case LogLevelOption::LOGGING_ERROR:
      return "ERROR";
    case LogLevelOption::LOGGING_CRITICAL:
      return "CRITICAL";
    case LogLevelOption::LOGGING_OFF:
      return "OFF";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize
