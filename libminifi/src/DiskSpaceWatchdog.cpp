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
#include <utility>

#include "core/Property.h"
#include "FlowController.h"
#include "core/logging/Logger.h"
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

Config read_config(const Configure& conf) {
  const auto interval_ms = conf.get(Configure::minifi_disk_space_watchdog_interval_ms) >>= string_to_milliseconds;
  const auto stop_bytes = conf.get(Configure::minifi_disk_space_watchdog_stop_threshold_bytes) >>= string_to_int<std::uintmax_t>;
  const auto restart_bytes = conf.get(Configure::minifi_disk_space_watchdog_restart_threshold_bytes) >>= string_to_int<std::uintmax_t>;
  if (restart_bytes < stop_bytes) { throw std::runtime_error{"disk space watchdog stop threshold must be <= restart threshold"}; }
  constexpr auto mebibytes = 1024 * 1024;
  return {
      interval_ms.value_or(chr::seconds{15}),
      stop_bytes.value_or(100 * mebibytes),
      restart_bytes.value_or(150 * mebibytes)
  };
}

struct callback {
  Config cfg;
  gsl::not_null<FlowController*> flow_controller;
  bool stopped;
  std::vector<std::string> paths_to_watch;
  std::shared_ptr<logging::Logger> logger;

  void operator()() {
    const auto stop_function = [this] { flow_controller->stop(); flow_controller->unload(); };
    const auto restart_function = [this] { flow_controller->load(); flow_controller->start(); };
    check(stop_function, restart_function);
  }

  template<typename StopFunc, typename RestartFunc>
  void check(StopFunc stop, RestartFunc restart) {
    const auto path_spaces = get_path_spaces();
    const auto has_insufficient_space = [this](utils::file::space_info space_info) { return space_info.available < cfg.stop_threshold_bytes; };
    const auto has_enough_space = [this](utils::file::space_info space_info) { return space_info.available > cfg.restart_threshold_bytes; };
    if (!stopped && std::any_of(std::begin(path_spaces), std::end(path_spaces), has_insufficient_space)) {
      logger->log_warn("Stopping flow controller due to insufficient disk space");
      stop();
      stopped = true;
    } else if (stopped && std::all_of(std::begin(path_spaces), std::end(path_spaces), has_enough_space)) {
      logger->log_info("Restarting flow controller");
      restart();
      stopped = false;
    }
  }

 private:
  std::vector<utils::file::space_info> get_path_spaces() const {
    std::vector<utils::file::space_info> result;
    result.reserve(paths_to_watch.size());
    const auto space = [this](const std::string& path) {
      try {
        const auto result = utils::file::space(path.c_str());
        logger->log_trace("%s available space: %zu bytes", path, gsl::narrow_cast<size_t>(result.available));
        return result;
      } catch (const std::exception& e) {
        logger->log_info("Couldn't check available disk space at %s: %s (ignoring)", path, e.what());
        constexpr auto kErrVal = gsl::narrow_cast<std::uintmax_t>(-1);
        return utils::file::space_info{kErrVal, kErrVal, kErrVal};
      }
    };
    std::transform(std::begin(paths_to_watch), std::end(paths_to_watch), std::back_inserter(result), space);
    return result;
  }

  bool has_insufficient_space(utils::file::space_info space_info) const noexcept { return space_info.available < cfg.stop_threshold_bytes; }
  bool has_enough_space(utils::file::space_info space_info) const noexcept { return space_info.available > cfg.restart_threshold_bytes; }
};

callback make_callback_and_check(Config cfg, FlowController& flow_controller, std::vector<std::string>&& paths_to_watch, std::shared_ptr<logging::Logger>&& logger) {
  callback cb{cfg, gsl::make_not_null(&flow_controller), false, std::move(paths_to_watch), std::move(logger)};
  cb.check([]{ throw std::runtime_error{"Insufficient disk space, MiNiFi is unable to start"}; }, []{});
  return cb;
}

}  // namespace

DiskSpaceWatchdog::DiskSpaceWatchdog(FlowController& flow_controller, const Configure& configure, std::vector<std::string> paths_to_watch)
  :utils::CallBackTimer{read_config(configure).interval, make_callback_and_check(read_config(configure), flow_controller, std::move(paths_to_watch),
      logging::LoggerFactory<DiskSpaceWatchdog>::getLogger())}
{ }

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
