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

#include "core/logging/BaseLogger.h"
#include <utility>
#include <memory>
#include <algorithm>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {

// Logger related configuration items.
const char *BaseLogger::nifi_log_level = "nifi.log.level";
const char *BaseLogger::nifi_log_appender = "nifi.log.appender";


// overridables

/**
 * @brief Log error message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
void BaseLogger::log_str(LOG_LEVEL_E level, const std::string &buffer) {
  switch (level) {
    case err:
    case critical:
      if (stderr_ != nullptr) {
        stderr_->error(buffer);
      } else {
        logger_->error(buffer);
      }
      break;
    case warn:
      logger_->warn(buffer);
      break;
    case info:
      logger_->info(buffer);
      break;
    case debug:
      logger_->debug(buffer);
      break;
    case trace:
      logger_->trace(buffer);
      break;
    case off:
      break;
    default:
      logger_->info(buffer);
      break;
  }
}

void BaseLogger::setLogLevel(const std::string &level,
                             LOG_LEVEL_E defaultLevel) {
  std::string logLevel = level;
  std::transform(logLevel.begin(), logLevel.end(), logLevel.begin(), ::tolower);

  if (logLevel == "trace") {
    setLogLevel(trace);
  } else if (logLevel == "debug") {
    setLogLevel(debug);
  } else if (logLevel == "info") {
    setLogLevel(info);
  } else if (logLevel == "warn") {
    setLogLevel(warn);
  } else if (logLevel == "error") {
    setLogLevel(err);
  } else if (logLevel == "critical") {
    setLogLevel(critical);
  } else if (logLevel == "off") {
    setLogLevel(off);
  } else {
    setLogLevel(defaultLevel);
  }
}

void BaseLogger::set_error_logger(std::shared_ptr<spdlog::logger> other) {
  stderr_ = std::move(other);
}

} /* namespace logging */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
