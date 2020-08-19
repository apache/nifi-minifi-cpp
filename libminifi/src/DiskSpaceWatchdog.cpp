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
#include "DiskSpaceWatchdog.h"

#include <chrono>
#include <cinttypes>
#include <type_traits>

#include "core/Property.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "utils/file/PathUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

namespace {
namespace chr = std::chrono;
struct Config {
  chr::milliseconds interval;
  std::uintmax_t stop_threshold_bytes;
  std::uintmax_t restart_threshold_bytes;
};

utils::optional<chr::milliseconds> string_to_milliseconds(const std::string& str) {
  uint64_t millisec_value{};
  const bool success = core::Property::getTimeMSFromString(str, millisec_value);
  if (!success) return utils::nullopt;
  return utils::make_optional(chr::milliseconds{millisec_value});
}

template<typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
utils::optional<T> string_to_int(const std::string& str) {
  T result{};
  const bool success = core::Property::StringToInt(str, result);
  if (!success) return utils::nullopt;
  return utils::make_optional(result);
}

Config read_config(const Configure* const conf) {
  const auto interval_ms = conf->get(Configure::minifi_disk_space_watchdog_interval_ms) >>= string_to_milliseconds;
  const auto stop_bytes = conf->get(Configure::minifi_disk_space_watchdog_stop_threshold_bytes) >>= string_to_int<std::uintmax_t>;
  const auto restart_bytes = conf->get(Configure::minifi_disk_space_watchdog_restart_threshold_bytes) >>= string_to_int<std::uintmax_t>;
  if (restart_bytes < stop_bytes) { throw std::runtime_error{"disk space watchdog stop threshold must be <= restart threshold"}; }
  constexpr auto mebibytes = 1024 * 1024;
  return {
      interval_ms.value_or(chr::milliseconds{100}),
      stop_bytes.value_or(15 * mebibytes),
      restart_bytes.value_or(20 * mebibytes)
  };
}

struct callback {
  Config cfg;
  FlowController* flow_controller;
  bool stopped;

  void operator()() {
    const auto spaceinfo = utils::file::space(".");
    if (!stopped && spaceinfo.available < cfg.stop_threshold_bytes) {
      flow_controller->stop(false);
      stopped = true;
    } else if (stopped && spaceinfo.available > cfg.restart_threshold_bytes) {
      flow_controller->start();
      stopped = false;
    }
  }
};

}  // namespace

DiskSpaceWatchdog::DiskSpaceWatchdog(FlowController* const flow_controller, const Configure* const configure)
  :utils::CallBackTimer{read_config(configure).interval, callback{read_config(configure), flow_controller, false}}
{ }

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
