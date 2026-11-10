// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <chrono>
#include <cinttypes>
#include <string>
#include <vector>

#include "utils/IntervalSwitch.h"

namespace org::apache::nifi::minifi {

class Configure;
namespace core::logging {
class Logger;
}  // namespace core::logging

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
}  // namespace org::apache::nifi::minifi
