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

namespace org::apache::nifi::minifi::aws::utils {

namespace {
Aws::Utils::Logging::LogLevel mapToAwsLevels(core::logging::LOG_LEVEL level) {
  switch (level) {
    case core::logging::trace: return Aws::Utils::Logging::LogLevel::Trace;
    case core::logging::debug: return Aws::Utils::Logging::LogLevel::Debug;
    case core::logging::info: return Aws::Utils::Logging::LogLevel::Info;
    case core::logging::warn: return Aws::Utils::Logging::LogLevel::Warn;
    case core::logging::err: return Aws::Utils::Logging::LogLevel::Error;
    case core::logging::critical: return Aws::Utils::Logging::LogLevel::Fatal;
    case core::logging::off: return Aws::Utils::Logging::LogLevel::Off;
    default:
      throw std::invalid_argument(fmt::format("Invalid LOG_LEVEL {}", magic_enum::enum_underlying(level)));
  }
}

core::logging::LOG_LEVEL mapFromAwsLevels(Aws::Utils::Logging::LogLevel level) {
  switch (level) {
    case Aws::Utils::Logging::LogLevel::Off: return core::logging::off;
    case Aws::Utils::Logging::LogLevel::Fatal: return core::logging::critical;
    case Aws::Utils::Logging::LogLevel::Error:return core::logging::err;
    case Aws::Utils::Logging::LogLevel::Warn: return core::logging::warn;
    case Aws::Utils::Logging::LogLevel::Info: return core::logging::info;
    case Aws::Utils::Logging::LogLevel::Debug: return core::logging::debug;
    case Aws::Utils::Logging::LogLevel::Trace: return core::logging::trace;
    default:
      throw std::invalid_argument(fmt::format("Invalid Aws::Utils::Logging::LogLevel {}", magic_enum::enum_underlying(level)));
  }
}
}  // namespace

Aws::Utils::Logging::LogLevel AWSSdkLogger::GetLogLevel() const {
  return mapToAwsLevels(logger_->level());
}

void AWSSdkLogger::Log(Aws::Utils::Logging::LogLevel log_level, const char* tag, const char* format_str, ...) {  // NOLINT(cert-dcl50-cpp)
  logger_->log_with_level(mapFromAwsLevels(log_level), "[{}] {}", tag, format_str);
}

void AWSSdkLogger::LogStream(Aws::Utils::Logging::LogLevel log_level, const char* tag, const Aws::OStringStream &message_stream) {
  Log(log_level, tag, message_stream.str().c_str());
}

void AWSSdkLogger::Flush() {
}

}  // namespace org::apache::nifi::minifi::aws::utils
