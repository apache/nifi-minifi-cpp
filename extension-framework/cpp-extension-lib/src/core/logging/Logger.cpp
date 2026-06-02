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

void CffiLogger::set_max_log_size(const int) {
  throw std::runtime_error("Unimplemented C Api");
}

void CffiLogger::log_string(const minifi::core::logging::LOG_LEVEL level, const std::string str) {
  minifi_logger_log_string(impl_, toCLogLevel(level), minifi_string_view{.data = str.data(), .length = str.length()});
}

bool CffiLogger::should_log(const minifi::core::logging::LOG_LEVEL level) {
  return minifi_logger_should_log(impl_, toCLogLevel(level));
}

[[nodiscard]] minifi::core::logging::LOG_LEVEL CffiLogger::level() const {
  throw std::runtime_error("Unimplemented C API");
}


}  // namespace org::apache::nifi::minifi::api::core::logging
