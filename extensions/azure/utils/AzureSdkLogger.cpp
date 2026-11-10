// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
