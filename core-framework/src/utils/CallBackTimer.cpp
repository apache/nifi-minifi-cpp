// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/CallBackTimer.h"
#include <stdexcept>

namespace org::apache::nifi::minifi::utils {

CallBackTimer::CallBackTimer(std::chrono::milliseconds interval, const std::function<void(void)>& func) : execute_(false), func_(func), interval_(interval) {
}

CallBackTimer::~CallBackTimer() {
  stop();
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
