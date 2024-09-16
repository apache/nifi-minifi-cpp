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

#include <cinttypes>

#include "controllers/keyvalue/AutoPersistor.h"
#include "core/TypedValues.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::controllers {

AutoPersistor::~AutoPersistor() {
  stop();
}

void AutoPersistor::stop() {
  std::unique_lock<std::mutex> lock(persisting_mutex_);
  if (persisting_thread_.joinable()) {
    running_ = false;
    lock.unlock();
    persisting_cv_.notify_one();
    persisting_thread_.join();
  }
}

void AutoPersistor::start(bool always_persist, std::chrono::milliseconds auto_persistence_interval, std::function<bool()> persist) {
  std::unique_lock<std::mutex> lock(persisting_mutex_);

  always_persist_ = always_persist;
  auto_persistence_interval_ = auto_persistence_interval;
  persist_ = std::move(persist);

  if (!always_persist_ && auto_persistence_interval_ != 0s) {
    if (!persisting_thread_.joinable()) {
      logger_->log_trace("Starting auto persistence thread");
      running_ = true;
      persisting_thread_ = std::thread(&AutoPersistor::persistingThreadFunc, this);
    }
  }

  logger_->log_trace("Enabled AutoPersistor");
}

void AutoPersistor::persistingThreadFunc() {
  std::unique_lock<std::mutex> lock(persisting_mutex_);

  while (true) {
    logger_->log_trace("Persisting thread is going to sleep for {}", auto_persistence_interval_);
    persisting_cv_.wait_for(lock, auto_persistence_interval_, [this] {
      return !running_;
    });

    if (!running_) {
      logger_->log_trace("Stopping persistence thread");
      return;
    }

    if (!persist_) {
      logger_->log_error("Persist function is empty");
    } else if (!persist_()) {
      logger_->log_error("Persisting failed");
    }
  }
}

}  // namespace org::apache::nifi::minifi::controllers
