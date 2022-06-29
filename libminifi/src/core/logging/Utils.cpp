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

#include "core/logging/Utils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils {

std::optional<spdlog::level::level_enum> parse_log_level(const std::string& level_name) {
  if (utils::StringUtils::equalsIgnoreCase(level_name, "trace")) {
    return spdlog::level::trace;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "debug")) {
    return spdlog::level::debug;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "info")) {
    return spdlog::level::info;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "warn")) {
    return spdlog::level::warn;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "error")) {
    return spdlog::level::err;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "critical")) {
    return spdlog::level::critical;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "off")) {
    return spdlog::level::off;
  }
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::utils
