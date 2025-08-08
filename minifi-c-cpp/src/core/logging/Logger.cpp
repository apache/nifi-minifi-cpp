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

#include "core/logging/Logger.h"

namespace org::apache::nifi::minifi::core::logging {

namespace {

MinifiLogLevel toCLogLevel(LOG_LEVEL lvl) {
  switch (lvl) {
    case trace: return MINIFI_TRACE;
    case debug: return MINIFI_DEBUG;
    case info: return MINIFI_INFO;
    case warn: return MINIFI_WARNING;
    case err: return MINIFI_ERROR;
    case critical: return MINIFI_CRITICAL;
    case off: return MINIFI_OFF;
  }
  gsl_FailFast();
}

LOG_LEVEL toLogLevel(MinifiLogLevel lvl) {
  switch (lvl) {
    case MINIFI_TRACE: return trace;
    case MINIFI_DEBUG: return debug;
    case MINIFI_INFO: return info;
    case MINIFI_WARNING: return warn;
    case MINIFI_ERROR: return err;
    case MINIFI_CRITICAL: return critical;
    case MINIFI_OFF: return off;
  }
  gsl_FailFast();
}

}  // namespace

void Logger::set_max_log_size(int size) {
  MinifiLoggerSetMaxLogSize(impl_, size);
}

std::optional<std::string> Logger::get_id() {
  std::optional<std::string> result;
  MinifiLoggerGetId(impl_, [] (void* data, MinifiStringView id) {
    (*(std::optional<std::string>*)data) = std::string_view{id.data, id.length};
  }, (void*)&result);
  return result;
}

void Logger::log_string(LOG_LEVEL level, std::string str) {
  MinifiLoggerLogString(impl_, toCLogLevel(level), MinifiStringView{.data = str.data(), .length = gsl::narrow_cast<uint32_t>(str.length())});
}

bool Logger::should_log(LOG_LEVEL level) {
  return MinifiLoggerShouldLog(impl_, toCLogLevel(level));
}

[[nodiscard]] LOG_LEVEL Logger::level() const {
  return toLogLevel(MinifiLoggerLevel(impl_));
}

int Logger::getMaxLogSize() {
  return MinifiLoggerGetMaxLogSize(impl_);
}


}  // nnamespace org::apache::nifi::minifi::cpp::core::logging
