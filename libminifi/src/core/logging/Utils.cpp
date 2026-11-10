// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/logging/Utils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils {

std::optional<spdlog::level::level_enum> parse_log_level(const std::string& level_name) {
  if (utils::string::equalsIgnoreCase(level_name, "trace")) {
    return spdlog::level::trace;
  } else if (utils::string::equalsIgnoreCase(level_name, "debug")) {
    return spdlog::level::debug;
  } else if (utils::string::equalsIgnoreCase(level_name, "info")) {
    return spdlog::level::info;
  } else if (utils::string::equalsIgnoreCase(level_name, "warn")) {
    return spdlog::level::warn;
  } else if (utils::string::equalsIgnoreCase(level_name, "error")) {
    return spdlog::level::err;
  } else if (utils::string::equalsIgnoreCase(level_name, "critical")) {
    return spdlog::level::critical;
  } else if (utils::string::equalsIgnoreCase(level_name, "off")) {
    return spdlog::level::off;
  }
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::utils
