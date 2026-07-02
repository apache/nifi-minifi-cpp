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

#pragma once

#include "minifi-cpp/core/logging/Logger.h"
namespace org::apache::nifi::minifi::core::logging {
class AdvancedLogger : public Logger {
 public:
  virtual void set_max_log_size(int size) = 0;
  [[nodiscard]] virtual LOG_LEVEL level() const = 0;

  static std::expected<void, std::error_code> setMaxLogSize(Logger* logger, int size) {
    if (const auto advanced_logger = dynamic_cast<AdvancedLogger*>(logger)) {
      advanced_logger->set_max_log_size(size);
      return {};
    }
    return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
  }

  static std::expected<LOG_LEVEL, std::error_code> getLevel(Logger* logger) {
    if (const auto advanced_logger = dynamic_cast<AdvancedLogger*>(logger)) {
      return advanced_logger->level();
    }
    return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
  }
};
}  // namespace org::apache::nifi::minifi::core::logging
