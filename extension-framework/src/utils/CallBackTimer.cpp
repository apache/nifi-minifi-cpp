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

namespace org::apache::nifi::minifi::utils {

CallBackTimer::CallBackTimer(std::chrono::milliseconds interval, const std::function<void(void)>& func) : execute_(false), func_(func), interval_(interval) {
}

CallBackTimer::~CallBackTimer() {
  stop();
  std::lock_guard<std::mutex> guard(mtx_);
  if (thd_.joinable()) {
    thd_.join();
  }
}

void CallBackTimer::stop() {
  std::lock_guard<std::mutex> guard(mtx_);
  {
    std::lock_guard<std::mutex> cv_guard(cv_mtx_);
    if (!execute_) {
      return;
    }
    execute_ = false;
    cv_.notify_all();
  }
}

void CallBackTimer::start() {
  std::lock_guard<std::mutex> guard(mtx_);
  {
    std::lock_guard<std::mutex> cv_guard(cv_mtx_);

    if (execute_) {
      return;
    }
  }

  if (thd_.joinable()) {
    thd_.join();
  }

  {
    std::lock_guard<std::mutex> cv_guard(cv_mtx_);
    execute_ = true;
  }

  thd_ = std::thread([this]() {
                       std::unique_lock<std::mutex> lk(cv_mtx_);
                       while (execute_) {
                         if (cv_.wait_for(lk, interval_, [this]{return !execute_;})) {
                           break;
                         }
                         lk.unlock();
                         this->func_();
                         lk.lock();
                       }
                     });
}

bool CallBackTimer::is_running() const {
  std::lock_guard<std::mutex> guard(mtx_);
  return execute_ && thd_.joinable();
}

}  // namespace org::apache::nifi::minifi::utils
