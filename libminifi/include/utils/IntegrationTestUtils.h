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

#include <utility>
#include <string>
#include <vector>

#include "../../../libminifi/test/TestBase.h"

namespace org::apache::nifi::minifi::utils {

template <class Rep, class Period, typename Fun>
bool verifyEventHappenedInPollTime(
    const std::chrono::duration<Rep, Period>& wait_duration,
    Fun&& check,
    std::chrono::microseconds check_interval = std::chrono::milliseconds(100)) {
  std::chrono::steady_clock::time_point wait_end = std::chrono::steady_clock::now() + wait_duration;
  do {
    if (std::forward<Fun>(check)()) {
      return true;
    }
    std::this_thread::sleep_for(check_interval);
  } while (std::chrono::steady_clock::now() < wait_end);
  return false;
}

template <class Rep, class Period, typename ...String>
bool verifyLogLinePresenceInPollTime(const std::chrono::duration<Rep, Period>& wait_duration, String&&... patterns) {
  auto check = [&patterns...] {
    const std::string logs = LogTestController::getInstance().getLogs();
    return ((logs.find(patterns) != std::string::npos) && ...);
  };
  return verifyEventHappenedInPollTime(wait_duration, check);
}

}  // namespace org::apache::nifi::minifi::utils
