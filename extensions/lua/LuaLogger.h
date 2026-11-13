// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>

#include "sol/sol.hpp"

namespace org::apache::nifi::minifi::extensions::lua {

class LuaLogger {
 public:
  explicit LuaLogger(gsl::not_null<core::logging::Logger*> logger) : logger_(logger) {}

  void log_info(std::string_view log_message) { logger_->log_info("{}", log_message); }
 private:
  gsl::not_null<core::logging::Logger*> logger_;
};

}  // namespace org::apache::nifi::minifi::extensions::lua
