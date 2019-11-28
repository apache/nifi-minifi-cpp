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
  std::unique_lock<std::mutex> lk(mtx_);
  if (execute_ || thd_.joinable()) {
    stop_inner(lk);
  }
}

void CallBackTimer::stop() {
  std::unique_lock<std::mutex> lk(mtx_);
  stop_inner(lk);
}

void CallBackTimer::start(const std::function<void(void)>& func) {
  std::unique_lock<std::mutex> lk(mtx_);
  if (execute_) {
    stop_inner(lk);
    lk.lock();
  }

  execute_ = true;
  thd_ = std::thread([this, func]() {
                       std::unique_lock<std::mutex> lk(mtx_);
                       while (execute_) {
                         if (cv_.wait_for(lk, interval_, [this]{return !execute_;})) {
                           break;
                         }
                         func();
                       }
                     });
}

bool CallBackTimer::is_running() const {
  std::lock_guard<std::mutex> guard(mtx_);
  return execute_ && thd_.joinable();
}


void CallBackTimer::stop_inner(std::unique_lock<std::mutex>& lk) {  // Private to make sure it's only called when the mutex is acquired
  if (!lk.owns_lock()) {
    throw std::invalid_argument("CallBackTimer::stop_inner was called without owning the provided lock!");
  }

  execute_ = false;
  cv_.notify_all();

  lk.unlock();

  if (thd_.joinable()) {
    thd_.join();
  }

}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
