// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <filesystem>

#include "core/logging/LoggerConfiguration.h"

#ifdef WIN32
extern "C" {
  FILE* __cdecl _imp____iob_func();

  FILE* __cdecl __imp___iob_func();
}
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

//! Main thread sleep interval 1 second
#define SLEEP_INTERVAL 1
//! Main thread stop wait time
#define STOP_WAIT_TIME_MS 30*1000
//! Default YAML location

#ifdef _MSC_VER
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#endif

namespace org::apache::nifi::minifi {
struct Locations {
  std::filesystem::path working_dir_;
  std::filesystem::path lock_path_;
  std::filesystem::path log_properties_path_;
  std::filesystem::path uid_properties_path_;
  std::filesystem::path properties_path_;
  std::filesystem::path logs_dir_;
  std::filesystem::path fips_bin_path_;
  std::filesystem::path fips_conf_path_;
};

/**
 * Configures the logger to log everything to syslog/Windows Event Log, and for the minimum log level to INFO
 */
void setSyslogLogger();

std::filesystem::path determineMinifiHome(const std::shared_ptr<org::apache::nifi::minifi::core::logging::Logger>& logger);

std::optional<Locations> determineLocations(const std::shared_ptr<org::apache::nifi::minifi::core::logging::Logger>& logger);

inline auto format_as(const Locations& loc) {
  static constexpr std::string_view fmt_str =
      "\n"
      "{:<24} {}\n"
      "{:<24} {}\n"
      "{:<24} {}\n"
      "{:<24} {}\n"
      "{:<24} {}\n"
      "{:<24} {}\n"
      "{:<24} {}\n"
      "{:<24} {}";

  return fmt::format(fmt_str,
      "Working dir:",         loc.working_dir_,
      "Lock path:",           loc.lock_path_,
      "Log properties path:", loc.log_properties_path_,
      "UID properties path:", loc.uid_properties_path_,
      "Properties path:",     loc.properties_path_,
      "Logs dir:",            loc.logs_dir_,
      "Fips binary path:",    loc.fips_bin_path_,
      "Fips conf path:",      loc.fips_conf_path_);
}
}  // namespace org::apache::nifi::minifi
