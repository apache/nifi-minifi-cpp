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
  LOGGING_OFF
};

template<typename... Args>
void logWithLevel(const std::shared_ptr<core::logging::Logger>& logger, LogLevelOption log_level, Args&&... args) {
  switch (log_level) {
    case LogLevelOption::LOGGING_TRACE:
      logger->log_trace(std::forward<Args>(args)...);
      break;
    case LogLevelOption::LOGGING_DEBUG:
      logger->log_debug(std::forward<Args>(args)...);
      break;
    case LogLevelOption::LOGGING_INFO:
      logger->log_info(std::forward<Args>(args)...);
      break;
    case LogLevelOption::LOGGING_WARN:
      logger->log_warn(std::forward<Args>(args)...);
      break;
    case LogLevelOption::LOGGING_ERROR:
      logger->log_error(std::forward<Args>(args)...);
      break;
    case LogLevelOption::LOGGING_OFF:
    default:
      break;
  }
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
    case LogLevelOption::LOGGING_OFF:
      return "OFF";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize
