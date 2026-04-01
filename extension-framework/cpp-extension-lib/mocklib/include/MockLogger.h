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

#pragma once

#include <map>
#include <string>

#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::mock {
class MockLogger : public core::logging::Logger {
 public:
  void set_max_log_size(int) override {}
  void log_string(const core::logging::LOG_LEVEL level, std::string s) override { logs_[level].emplace_back(std::move(s)); }
  [[nodiscard]] bool should_log(const core::logging::LOG_LEVEL level) override { return level > log_level_; }
  [[nodiscard]] core::logging::LOG_LEVEL level() const override { return log_level_; }

  core::logging::LOG_LEVEL log_level_ = core::logging::LOG_LEVEL::trace;
  std::map<core::logging::LOG_LEVEL, std::vector<std::string>> logs_;
};
}  // namespace org::apache::nifi::minifi::mock
