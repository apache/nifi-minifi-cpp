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
#include "controllers/LinuxPowerManagementService.h"

#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "utils/gsl.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

core::Property LinuxPowerManagerService::BatteryCapacityPath(
    core::PropertyBuilder::createProperty("Battery Capacity Path")->withDescription("Path to the battery level")->isRequired(true)->withDefaultValue<std::string>(
        "/sys/class/power_supply/BAT0/capacity")->build());

core::Property LinuxPowerManagerService::BatteryStatusPath(
    core::PropertyBuilder::createProperty("Battery Status Path")->withDescription("Path to the battery status ( Discharging/Battery )")->isRequired(true)->withDefaultValue<std::string>(
        "/sys/class/power_supply/BAT0/status")->build());

core::Property LinuxPowerManagerService::BatteryStatusDischargeKeyword(
    core::PropertyBuilder::createProperty("Battery Status Discharge")->withDescription("Keyword to identify if battery is discharging")->isRequired(true)->withDefaultValue<std::string>("Discharging")
        ->build());

core::Property LinuxPowerManagerService::TriggerThreshold(
    core::PropertyBuilder::createProperty("Trigger Threshold")->withDescription("Battery threshold before which we consider a slow reduction. Should be a number from 1-100")->isRequired(true)
        ->withDefaultValue<int>(75)->build());

core::Property LinuxPowerManagerService::WaitPeriod(
    core::PropertyBuilder::createProperty("Wait Period")->withDescription("Decay between checking threshold and determining if a reduction is needed")->isRequired(true)
        ->withDefaultValue<core::TimePeriodValue>("100 ms")->build());

core::Property LinuxPowerManagerService::LowBatteryThreshold(
    core::PropertyBuilder::createProperty("Low Battery Threshold")->withDescription("Battery threshold before which we will aggressively reduce. Should be a number from 1-100")->isRequired(true)
        ->withDefaultValue<int>(50)->build());

bool LinuxPowerManagerService::isAboveMax(int /*new_tasks*/) {
  return false;
}

uint16_t LinuxPowerManagerService::getMaxThreads() {
  return std::numeric_limits<uint16_t>::max();
}

bool LinuxPowerManagerService::canIncrease() {
  for (const auto& path_pair : paths_) {
    try {
      auto capacity = path_pair.first;
      auto status = path_pair.second;

      std::ifstream status_file(status);
      std::string status_str;
      std::getline(status_file, status_str);
      status_file.close();

      if (!utils::StringUtils::equalsIgnoreCase(status_keyword_, status_str)) {
        return true;
      }
    } catch (...) {
      logger_->log_error("Could not read file paths. ignoring, temporarily");
    }
  }
  return false;
}

void LinuxPowerManagerService::reduce() {
  auto curr_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
  last_time_ = curr_time;
}

/**
 * We expect that the wait period has been
 */
bool LinuxPowerManagerService::shouldReduce() {
  if (!enabled_) {
    logger_->log_trace("LPM not enabled");
    return false;
  }

  bool overConsume = false;

  std::vector<bool> batteryAlert;

  auto prev_level = battery_level_.load();

  bool all_discharging = !paths_.empty();

  int battery_sum = 0;
  for (const auto& path_pair : paths_) {
    try {
      auto capacity = path_pair.first;
      auto status = path_pair.second;

      std::ifstream capacity_file(capacity);
      std::string capacity_str;
      std::getline(capacity_file, capacity_str);
      capacity_file.close();
      int battery_level = std::stoi(capacity_str);
      battery_sum += battery_level;

      std::ifstream status_file(status);
      std::string status_str;
      std::getline(status_file, status_str);
      status_file.close();

      if (!utils::StringUtils::equalsIgnoreCase(status_keyword_, status_str)) {
        all_discharging &= false;
      }
    } catch (...) {
      logger_->log_error("Error caught while pulling paths");
      return false;
    }
  }

  // average
  battery_level_ = battery_sum / gsl::narrow<int>(paths_.size());

  // only reduce if we're still going down OR we've triggered the low battery threshold
  if (battery_level_ < trigger_ && (battery_level_ < prev_level || battery_level_ < low_battery_trigger_)) {
    if (all_discharging) {
      // return true and wait until
      if (last_time_ == 0) {
        overConsume = true;
        last_time_ = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
      } else {
        auto curr_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
        if (curr_time - last_time_ > wait_period_) {
          overConsume = true;
          logger_->log_trace("All banks are discharging, suggesting reduction");
        } else {
          logging::LOG_DEBUG(logger_) << "dischaging but can't reduce due to time " << curr_time << " " << last_time_ << "  " << wait_period_;
        }
      }
    }

  } else {
    logger_->log_trace("%d level is not below trigger of %d", battery_level_.load(), trigger_);
  }

  return overConsume;
}

void LinuxPowerManagerService::initialize() {
  ThreadManagementService::initialize();
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(BatteryCapacityPath);
  supportedProperties.insert(BatteryStatusPath);
  supportedProperties.insert(TriggerThreshold);
  supportedProperties.insert(LowBatteryThreshold);
  supportedProperties.insert(WaitPeriod);
  setSupportedProperties(supportedProperties);
}

void LinuxPowerManagerService::yield() {
}

bool LinuxPowerManagerService::isRunning() {
  return getState() == core::controller::ControllerServiceState::ENABLED;
}

bool LinuxPowerManagerService::isWorkAvailable() {
  return false;
}

void LinuxPowerManagerService::onEnable() {
  if (nullptr == configuration_) {
    logger_->log_trace("Cannot enable Linux Power Manager");
    return;
  }
  status_keyword_ = "Discharging";
  core::Property capacityPaths;
  core::Property statusPaths;

  uint64_t wait;
  if (getProperty(TriggerThreshold.getName(), trigger_) && getProperty(WaitPeriod.getName(), wait)) {
    wait_period_ = wait;

    getProperty(BatteryStatusDischargeKeyword.getName(), status_keyword_);

    if (!getProperty(LowBatteryThreshold.getName(), low_battery_trigger_)) {
      low_battery_trigger_ = 0;
    }
    getProperty(BatteryCapacityPath.getName(), capacityPaths);
    getProperty(BatteryStatusPath.getName(), statusPaths);
    if (capacityPaths.getValues().size() == statusPaths.getValues().size()) {
      for (size_t i = 0; i < capacityPaths.getValues().size(); i++) {
        paths_.push_back(std::make_pair(capacityPaths.getValues().at(i), statusPaths.getValues().at(i)));
      }
    } else {
      logger_->log_error("BatteryCapacityPath and BatteryStatusPath mis-configuration");
    }

    enabled_ = true;
    logger_->log_trace("Enabled enable ");
  } else {
    logger_->log_trace("Could not enable ");
  }
}

REGISTER_RESOURCE(LinuxPowerManagerService, "Linux power management service that enables control of power usage in the agent through Linux power management information. Use name \"ThreadPoolManager\" to throttle battery consumption"); // NOLINT

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
