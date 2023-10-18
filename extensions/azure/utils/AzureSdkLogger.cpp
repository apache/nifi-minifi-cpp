/**
 * @file AzureSdkLogger.cpp
 * AzureSdkLogger class implementation
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

#include "AzureSdkLogger.h"

#include <string>

#include "azure/core/diagnostics/logger.hpp"

namespace org::apache::nifi::minifi::azure::utils {

void AzureSdkLogger::initialize() {
  static AzureSdkLogger instance;
}

void AzureSdkLogger::setLogLevel() {
  if (logger_->should_log(minifi::core::logging::LOG_LEVEL::trace) || logger_->should_log(minifi::core::logging::LOG_LEVEL::debug)) {
    Azure::Core::Diagnostics::Logger::SetLevel(Azure::Core::Diagnostics::Logger::Level::Verbose);
  } else if (logger_->should_log(minifi::core::logging::LOG_LEVEL::info)) {
    Azure::Core::Diagnostics::Logger::SetLevel(Azure::Core::Diagnostics::Logger::Level::Informational);
  } else if (logger_->should_log(minifi::core::logging::LOG_LEVEL::warn)) {
    Azure::Core::Diagnostics::Logger::SetLevel(Azure::Core::Diagnostics::Logger::Level::Warning);
  } else if (logger_->should_log(minifi::core::logging::LOG_LEVEL::err) || logger_->should_log(minifi::core::logging::LOG_LEVEL::critical)) {
    Azure::Core::Diagnostics::Logger::SetLevel(Azure::Core::Diagnostics::Logger::Level::Error);
  }
}

AzureSdkLogger::AzureSdkLogger() {
  setLogLevel();

  Azure::Core::Diagnostics::Logger::SetListener([&](Azure::Core::Diagnostics::Logger::Level level, const std::string& message) {
    switch (level) {
      case Azure::Core::Diagnostics::Logger::Level::Verbose:
        logger_->log_debug("{}", message);
        break;
      case Azure::Core::Diagnostics::Logger::Level::Informational:
        logger_->log_info("{}", message);
        break;
      case Azure::Core::Diagnostics::Logger::Level::Warning:
        logger_->log_warn("{}", message);
        break;
      case Azure::Core::Diagnostics::Logger::Level::Error:
        logger_->log_error("{}", message);
        break;
      default:
        break;
    }
  });
}

}  // namespace org::apache::nifi::minifi::azure::utils
