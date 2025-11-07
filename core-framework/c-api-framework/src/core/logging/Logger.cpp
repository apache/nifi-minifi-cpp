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

MinifiLogLevel toCLogLevel(minifi::core::logging::LOG_LEVEL lvl) {
  switch (lvl) {
    case minifi::core::logging::trace: return MINIFI_TRACE;
    case minifi::core::logging::debug: return MINIFI_DEBUG;
    case minifi::core::logging::info: return MINIFI_INFO;
    case minifi::core::logging::warn: return MINIFI_WARNING;
    case minifi::core::logging::err: return MINIFI_ERROR;
    case minifi::core::logging::critical: return MINIFI_CRITICAL;
    case minifi::core::logging::off: return MINIFI_OFF;
  }
  gsl_FailFast();
}

}  // namespace

void Logger::set_max_log_size(int size) {
  MinifiLoggerSetMaxLogSize(impl_, size);
}

void Logger::log_string(minifi::core::logging::LOG_LEVEL level, std::string str) {
  MinifiLoggerLogString(impl_, toCLogLevel(level), MinifiStringView{.data = str.data(), .length = gsl::narrow_cast<uint32_t>(str.length())});
}

bool Logger::should_log(minifi::core::logging::LOG_LEVEL level) {
  return MinifiLoggerShouldLog(impl_, toCLogLevel(level));
}


}  // namespace org::apache::nifi::minifi::api::core::logging
