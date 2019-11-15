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

CallBackTimer::CallBackTimer(int interval) : execute_(false), interval_(interval) {
  if (interval_ <= 0) {
    throw std::invalid_argument("Interval cannot be negative!");
  }
}

CallBackTimer::~CallBackTimer() {
  if (execute_.load(std::memory_order_acquire) || thd_.joinable()) {
    stop();
  }
}

void CallBackTimer::stop() {
  execute_.store(false, std::memory_order_release);
  if (thd_.joinable()) {
    thd_.join();
  }
}

void CallBackTimer::start(std::function<void(void)> func) {
  if (execute_.load(std::memory_order_acquire)) {
    stop();
  }

  execute_.store(true, std::memory_order_release);
  thd_ = std::thread([this, func]() {
                       auto time = std::chrono::milliseconds(interval_);
                       std::this_thread::sleep_for(time);
                       while (execute_.load(std::memory_order_acquire)) {
                         func();
                         std::this_thread::sleep_for(time);
                       }
                     });
}

bool CallBackTimer::is_running() const {
  return execute_.load(std::memory_order_acquire) && thd_.joinable();
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
