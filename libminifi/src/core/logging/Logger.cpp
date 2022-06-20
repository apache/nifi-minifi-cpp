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

#include "core/logging/Logger.h"

#include <mutex>
#include <memory>
#include <sstream>
#include <iostream>

namespace org::apache::nifi::minifi::core::logging {

LoggerControl::LoggerControl()
    : is_enabled_(true) {
}

bool LoggerControl::is_enabled() const {
  return is_enabled_;
}

void LoggerControl::setEnabled(bool status) {
  is_enabled_ = status;
}


BaseLogger::~BaseLogger() = default;

bool BaseLogger::should_log(const LOG_LEVEL& /*level*/) {
  return true;
}

LogBuilder::LogBuilder(BaseLogger *l, LOG_LEVEL level)
    : ignore(false),
      ptr(l),
      level(level) {
  if (!l->should_log(level)) {
    setIgnore();
  }
}

LogBuilder::~LogBuilder() {
  if (!ignore)
    log_string(level);
}

void LogBuilder::setIgnore() {
  ignore = true;
}

void LogBuilder::log_string(LOG_LEVEL level) const {
  ptr->log_string(level, str.str());
}


bool Logger::should_log(const LOG_LEVEL &level) {
  if (controller_ && !controller_->is_enabled())
    return false;
  spdlog::level::level_enum logger_level = spdlog::level::level_enum::info;
  switch (level) {
    case critical:
      logger_level = spdlog::level::level_enum::critical;
      break;
    case err:
      logger_level = spdlog::level::level_enum::err;
      break;
    case info:
      break;
    case debug:
      logger_level = spdlog::level::level_enum::debug;
      break;
    case off:
      logger_level = spdlog::level::level_enum::off;
      break;
    case trace:
      logger_level = spdlog::level::level_enum::trace;
      break;
    case warn:
      logger_level = spdlog::level::level_enum::warn;
      break;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  return delegate_->should_log(logger_level);
}

void Logger::log_string(LOG_LEVEL level, std::string str) {
  switch (level) {
    case critical:
      log_warn(str.c_str());
      break;
    case err:
      log_error(str.c_str());
      break;
    case info:
      log_info(str.c_str());
      break;
    case debug:
      log_debug(str.c_str());
      break;
    case trace:
      log_trace(str.c_str());
      break;
    case warn:
      log_warn(str.c_str());
      break;
    case off:
      break;
  }
}

Logger::Logger(std::shared_ptr<spdlog::logger> delegate, std::shared_ptr<LoggerControl> controller)
    : delegate_(std::move(delegate)), controller_(std::move(controller)) {
}

Logger::Logger(std::shared_ptr<spdlog::logger> delegate)
    : delegate_(std::move(delegate)), controller_(nullptr) {
}

}  // namespace org::apache::nifi::minifi::core::logging
