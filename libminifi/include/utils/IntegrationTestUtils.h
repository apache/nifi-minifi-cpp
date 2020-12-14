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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

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
  // gcc before 4.9 does not support capturing parameter packs in lambdas: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=41933
  // Once we support gcc >= 4.9 only, this vector will no longer be necessary, we'll be able to iterate on the parameter pack directly.
  std::vector<std::string> pattern_list{std::forward<String>(patterns)...};
  auto check = [&] {
    const std::string logs = LogTestController::getInstance().log_output.str();
    return std::all_of(pattern_list.cbegin(), pattern_list.cend(), [&logs] (const std::string& pattern) { return logs.find(pattern) != std::string::npos; });
  };
  return verifyEventHappenedInPollTime(wait_duration, check);
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
