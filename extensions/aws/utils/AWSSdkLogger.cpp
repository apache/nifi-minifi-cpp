/**
 * @file AWSSdkLogger.cpp
 * AWSSdkLogger class implementation
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
#include "AWSSdkLogger.h"

#include "aws/core/utils/logging/LogLevel.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace utils {

Aws::Utils::Logging::LogLevel AWSSdkLogger::GetLogLevel() const {
  if (logger_->should_log(minifi::core::logging::LOG_LEVEL::trace))
    return Aws::Utils::Logging::LogLevel::Trace;
  if (logger_->should_log(minifi::core::logging::LOG_LEVEL::debug))
    return Aws::Utils::Logging::LogLevel::Debug;
  if (logger_->should_log(minifi::core::logging::LOG_LEVEL::info))
    return Aws::Utils::Logging::LogLevel::Info;
  if (logger_->should_log(minifi::core::logging::LOG_LEVEL::warn))
    return Aws::Utils::Logging::LogLevel::Warn;
  if (logger_->should_log(minifi::core::logging::LOG_LEVEL::err))
    return Aws::Utils::Logging::LogLevel::Error;
  if (logger_->should_log(minifi::core::logging::LOG_LEVEL::critical))
    return Aws::Utils::Logging::LogLevel::Fatal;
  return Aws::Utils::Logging::LogLevel::Off;
}

void AWSSdkLogger::Log(Aws::Utils::Logging::LogLevel log_level, const char* tag, const char* format_str, ...) { // NOLINT (cert-dcl50-cpp)
  switch (log_level) {
    case Aws::Utils::Logging::LogLevel::Trace:
      logger_->log_trace("[%s] %s", tag, format_str);
      break;
    case Aws::Utils::Logging::LogLevel::Debug:
      logger_->log_debug("[%s] %s", tag, format_str);
      break;
    case Aws::Utils::Logging::LogLevel::Info:
      logger_->log_info("[%s] %s", tag, format_str);
      break;
    case Aws::Utils::Logging::LogLevel::Warn:
      logger_->log_warn("[%s] %s", tag, format_str);
      break;
    case Aws::Utils::Logging::LogLevel::Error:
    case Aws::Utils::Logging::LogLevel::Fatal:
      logger_->log_error("[%s] %s", tag, format_str);
      break;
    default:
      break;
  }
}

void AWSSdkLogger::LogStream(Aws::Utils::Logging::LogLevel log_level, const char* tag, const Aws::OStringStream &message_stream) {
  Log(log_level, tag, message_stream.str().c_str());
}

void AWSSdkLogger::Flush() {
}

}  // namespace utils
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
