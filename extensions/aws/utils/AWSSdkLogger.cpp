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

#include <cstdio>
#include <cstdarg>

#include "aws/core/utils/logging/LogLevel.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::aws::utils {

namespace {
Aws::Utils::Logging::LogLevel mapToAwsLevels(core::logging::LOG_LEVEL level) {
  switch (level) {
    using core::logging::LOG_LEVEL;
    using AwsLogLevel = Aws::Utils::Logging::LogLevel;
    case LOG_LEVEL::trace: return AwsLogLevel::Trace;
    case LOG_LEVEL::debug: return AwsLogLevel::Debug;
    case LOG_LEVEL::info: return AwsLogLevel::Info;
    case LOG_LEVEL::warn: return AwsLogLevel::Warn;
    case LOG_LEVEL::err: return AwsLogLevel::Error;
    case LOG_LEVEL::critical: return AwsLogLevel::Fatal;
    case LOG_LEVEL::off: return AwsLogLevel::Off;
  }
  throw std::invalid_argument(fmt::format("Invalid LOG_LEVEL {}", magic_enum::enum_underlying(level)));
}

core::logging::LOG_LEVEL mapFromAwsLevels(Aws::Utils::Logging::LogLevel level) {
  switch (level) {
    using core::logging::LOG_LEVEL;
    using AwsLogLevel = Aws::Utils::Logging::LogLevel;
    case AwsLogLevel::Off: return LOG_LEVEL::off;
    case AwsLogLevel::Fatal: return LOG_LEVEL::critical;
    case AwsLogLevel::Error:return LOG_LEVEL::err;
    case AwsLogLevel::Warn: return LOG_LEVEL::warn;
    case AwsLogLevel::Info: return LOG_LEVEL::info;
    case AwsLogLevel::Debug: return LOG_LEVEL::debug;
    case AwsLogLevel::Trace: return LOG_LEVEL::trace;
  }
  throw std::invalid_argument(fmt::format("Invalid Aws::Utils::Logging::LogLevel {}", magic_enum::enum_underlying(level)));
}
}  // namespace

Aws::Utils::Logging::LogLevel AWSSdkLogger::GetLogLevel() const {
  return mapToAwsLevels(logger_->level());
}

void AWSSdkLogger::Log(Aws::Utils::Logging::LogLevel log_level, const char* tag, const char* format_str, ...) {  // NOLINT(cert-dcl50-cpp)
  va_list args;
  va_start(args, format_str);
  vaLog(log_level, tag, format_str, args);
  va_end(args);
}

void AWSSdkLogger::vaLog(Aws::Utils::Logging::LogLevel log_level, const char* tag, const char* format_str, va_list args) {
  va_list args_copy;
  va_copy(args_copy, args);
  const int buffer_size = std::vsnprintf(nullptr, 0, format_str, args_copy) + 1;  // +1 for the terminating \0 character
  va_end(args_copy);

  std::vector<char> buffer(buffer_size);

  const int length = std::vsnprintf(buffer.data(), buffer_size, format_str, args);
  if (length < 0) {
    logger_->log_error("A log line from aws-sdk-cpp could not be processed: [{}] {}", tag, format_str);
    return;
  }
  gsl_Assert(length <= buffer_size);

  log(log_level, tag, std::string_view(buffer.data(), length));
}

void AWSSdkLogger::LogStream(Aws::Utils::Logging::LogLevel log_level, const char* tag, const Aws::OStringStream& message_stream) {
  log(log_level, tag, message_stream.str());
}

void AWSSdkLogger::log(Aws::Utils::Logging::LogLevel log_level, const char* tag, std::string_view message) const {
  logger_->log_with_level(mapFromAwsLevels(log_level), "[{}] {}", tag, message);
}

void AWSSdkLogger::Flush() {
}

}  // namespace org::apache::nifi::minifi::aws::utils
