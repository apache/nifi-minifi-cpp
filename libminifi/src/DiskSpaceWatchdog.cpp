// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "DiskSpaceWatchdog.h"

#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "properties/Configure.h"
#include "utils/file/PathUtils.h"
#include "utils/TimeUtil.h"

namespace org::apache::nifi::minifi {

namespace {
namespace chr = std::chrono;

std::optional<uint64_t> data_size_string_to_int(const std::string& str) {
  return parsing::parseDataSize(str) | utils::toOptional();
}

}  // namespace

namespace disk_space_watchdog {
Config read_config(const Configure& conf) {
  const auto interval_ms = conf.get(Configure::minifi_disk_space_watchdog_interval) | utils::andThen(utils::timeutils::StringToDuration<chr::milliseconds>);
  const auto stop_bytes = conf.get(Configure::minifi_disk_space_watchdog_stop_threshold) | utils::andThen(data_size_string_to_int);
  const auto restart_bytes = conf.get(Configure::minifi_disk_space_watchdog_restart_threshold) | utils::andThen(data_size_string_to_int);
  if (restart_bytes < stop_bytes) { throw std::runtime_error{"disk space watchdog stop threshold must be <= restart threshold"}; }
  constexpr auto mebibytes = 1024 * 1024;
  return {
      interval_ms.value_or(chr::seconds{15}),
      stop_bytes.value_or(100 * mebibytes),
      restart_bytes.value_or(150 * mebibytes)
  };
}


std::vector<std::uintmax_t> check_available_space(const std::vector<std::string>& paths, core::logging::Logger* logger) {
  std::vector<std::uintmax_t> result;
  result.reserve(paths.size());
  std::transform(std::begin(paths), std::end(paths), std::back_inserter(result), [logger](const std::string& path) {
    std::error_code ec;
    const auto result = utils::file::space(path.c_str(), ec);
    if (ec && logger) {
      logger->log_info("Couldn't check disk space at {}: {} (ignoring)", path, ec.message());
    } else if (logger) {
      logger->log_trace("{} available space: {} bytes", path, gsl::narrow_cast<size_t>(result.available));
    }
    return result.available;
  });
  return result;
}

}  // namespace disk_space_watchdog
}  // namespace org::apache::nifi::minifi
