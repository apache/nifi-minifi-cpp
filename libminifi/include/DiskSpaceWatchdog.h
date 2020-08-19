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

#include <chrono>
#include <cinttypes>
#include <string>
#include <vector>

#include "utils/IntervalSwitch.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class Configure;
namespace core {
namespace logging {
class Logger;
}  // namespace logging
}  // namespace core

namespace disk_space_watchdog {
struct Config {
  std::chrono::milliseconds interval;
  std::uintmax_t stop_threshold_bytes;
  std::uintmax_t restart_threshold_bytes;
};

Config read_config(const Configure&);

inline utils::IntervalSwitch<std::uintmax_t> disk_space_interval_switch(Config config) {
  return {config.stop_threshold_bytes, config.restart_threshold_bytes, utils::IntervalSwitchState::UPPER};
}

// Esentially `paths | transform(utils::file::space) | transform(&utils::file::space_info::available)` with error logging
std::vector<std::uintmax_t> check_available_space(const std::vector<std::string>& paths, core::logging::Logger* logger = nullptr);

}  // namespace disk_space_watchdog
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
