/**
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
#include <chrono>

namespace org::apache::nifi::minifi::utils::timeutils {

/**
 * Mockable clock classes
 */
class Clock {
 public:
  virtual ~Clock() = default;
  virtual std::chrono::milliseconds timeSinceEpoch() const = 0;
  virtual bool wait_until(std::condition_variable& cv, std::unique_lock<std::mutex>& lck, std::chrono::milliseconds time, const std::function<bool()>& pred) {
    return cv.wait_for(lck, time - timeSinceEpoch(), pred);
  }
};

class SteadyClock : public Clock {
 public:
  std::chrono::milliseconds timeSinceEpoch() const override {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
  }

  virtual std::chrono::time_point<std::chrono::steady_clock> now() const {
    return std::chrono::steady_clock::now();
  }
};

std::shared_ptr<SteadyClock> getClock();

// test-only utility to specify what clock to use
void setClock(std::shared_ptr<SteadyClock> clock);

#ifdef WIN32
void dateSetGlobalInstall(const std::string& install);
#endif

}  // namespace org::apache::nifi::minifi::utils::timeutils

