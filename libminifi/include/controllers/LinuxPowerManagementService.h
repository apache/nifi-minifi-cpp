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

#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <memory>
#include "utils/StringUtils.h"
#include "io/validation.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerFactory.h"
#include "ThreadManagementService.h"

namespace org::apache::nifi::minifi::controllers {

/**
 * Purpose: Linux power management service uses a path for the battery level
 * and the status ( charging/discharging )
 */
class LinuxPowerManagerService : public ThreadManagementService {
 public:
  explicit LinuxPowerManagerService(std::string name, const utils::Identifier &uuid = {})
      : ThreadManagementService(std::move(name), uuid),
        enabled_(false),
        battery_level_(0),
        wait_period_(0),
        last_time_(0),
        trigger_(0),
        low_battery_trigger_(0) {
  }

  explicit LinuxPowerManagerService(std::string name, const std::shared_ptr<Configure> &configuration)
      : LinuxPowerManagerService(std::move(name)) {
    setConfiguration(configuration);
    initialize();
  }

  MINIFIAPI static constexpr const char* Description = "Linux power management service that enables control of power usage in the agent through Linux power management information. "
      "Use name \"ThreadPoolManager\" to throttle battery consumption";

  MINIFIAPI static const core::Property BatteryCapacityPath;
  MINIFIAPI static const core::Property BatteryStatusPath;
  MINIFIAPI static const core::Property BatteryStatusDischargeKeyword;
  MINIFIAPI static const core::Property TriggerThreshold;
  MINIFIAPI static const core::Property LowBatteryThreshold;
  MINIFIAPI static const core::Property WaitPeriod;
  static auto properties() {
    return std::array{
      BatteryCapacityPath,
      BatteryStatusPath,
      BatteryStatusDischargeKeyword,
      TriggerThreshold,
      LowBatteryThreshold,
      WaitPeriod
    };
  }

  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  /**
   * Helps to determine if the number of tasks will increase the pools above their threshold.
   * @param new_tasks tasks to be added.
   * @return true if above max, false otherwise.
   */
  bool isAboveMax(int new_tasks) override;

  /**
   * Returns the max number of threads allowed by all pools
   * @return max threads.
   */
  uint16_t getMaxThreads() override;

  /**
   * Function based on cooperative multitasking that will tell a caller whether or not the number of threads should be reduced.
   * @return true if threading impacts QOS.
   */
  bool shouldReduce() override;

  /**
   * Function to indicate to this controller service that we've reduced threads in a threadpool
   */
  void reduce() override;

  /**
   * Function to help callers identify if they can increase threads.
   * @return true if QOS won't be breached.
   */
  bool canIncrease() override;

  void initialize() override;

  void yield() override;

  bool isRunning() const override;

  bool isWorkAvailable() override;

  void onEnable() override;

 protected:
  std::vector<std::pair<std::string, std::string>> paths_;

  bool enabled_;

  std::atomic<int> battery_level_;

  std::atomic<uint64_t> wait_period_;

  std::atomic<uint64_t> last_time_;

  int trigger_;

  int low_battery_trigger_;

  std::string status_keyword_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<LinuxPowerManagerService>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::controllers
