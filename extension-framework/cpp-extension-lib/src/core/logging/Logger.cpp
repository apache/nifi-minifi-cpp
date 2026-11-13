// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "api/core/logging/Logger.h"

namespace org::apache::nifi::minifi::api::core::logging {

namespace {

minifi_log_level toCLogLevel(minifi::core::logging::LOG_LEVEL lvl) {
  switch (lvl) {
    case minifi::core::logging::trace: return MINIFI_LOG_LEVEL_TRACE;
    case minifi::core::logging::debug: return MINIFI_LOG_LEVEL_DEBUG;
    case minifi::core::logging::info: return MINIFI_LOG_LEVEL_INFO;
    case minifi::core::logging::warn: return MINIFI_LOG_LEVEL_WARNING;
    case minifi::core::logging::err: return MINIFI_LOG_LEVEL_ERROR;
    case minifi::core::logging::critical: return MINIFI_LOG_LEVEL_CRITICAL;
    case minifi::core::logging::off: return MINIFI_LOG_LEVEL_OFF;
  }
  gsl_FailFast();
}
}  // namespace


void CffiLogger::log_string(const minifi::core::logging::LOG_LEVEL level, const std::string str) {
  minifi_logger_log_string(impl_, toCLogLevel(level), minifi_string_view{.data = str.data(), .length = str.length()});
}

bool CffiLogger::should_log(const minifi::core::logging::LOG_LEVEL level) {
  return minifi_logger_should_log(impl_, toCLogLevel(level));
}

}  // namespace org::apache::nifi::minifi::api::core::logging
