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

#include "utils/CallBackTimer.h"
#include <stdexcept>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

CallBackTimer::CallBackTimer(std::chrono::milliseconds interval) : execute_(false), interval_(interval) {
}

CallBackTimer::~CallBackTimer() {
  std::lock_guard<std::recursive_mutex> guard(mtx_);
  if (execute_ || thd_.joinable()) {
    stop();
  }
}

void CallBackTimer::stop() {
  std::lock_guard<std::recursive_mutex> guard(mtx_);
  execute_ = false;
  if (thd_.joinable()) {
    thd_.join();
  }
}

void CallBackTimer::start(std::function<void(void)> func) {
  std::lock_guard<std::recursive_mutex> guard(mtx_);
  if (execute_) {
    stop();
  }

  execute_ = true;
  thd_ = std::thread([this, func]() {
                       std::this_thread::sleep_for(interval_);
                       while (execute_) {
                         func();
                         std::this_thread::sleep_for(interval_);
                       }
                     });
}

bool CallBackTimer::is_running() const {
  std::lock_guard<std::recursive_mutex> guard(mtx_);
  return execute_ && thd_.joinable();
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
