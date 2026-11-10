// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <map>
#include <string>

#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::mock {
class MockLogger : public core::logging::Logger {
 public:
  void log_string(const core::logging::LOG_LEVEL level, std::string s) override { logs_[level].emplace_back(std::move(s)); }
  [[nodiscard]] bool should_log(const core::logging::LOG_LEVEL level) override { return level >= log_level_; }

  core::logging::LOG_LEVEL log_level_ = core::logging::LOG_LEVEL::trace;
  std::map<core::logging::LOG_LEVEL, std::vector<std::string>> logs_;
};
}  // namespace org::apache::nifi::minifi::mock
