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

#include <algorithm>
#include <atomic>
#include <chrono>
#if defined(WIN32)
#include <future>  // This is required to work around a VS2017 bug, see the details below
#endif
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::utils {

/**
 * Worker task helper that determines
 * whether or not we will run
 */
template<typename T>
class AfterExecute {
 public:
  virtual ~AfterExecute() = default;

  AfterExecute() = default;
  AfterExecute(AfterExecute&& /*other*/)  noexcept = default;
  virtual bool isFinished(const T &result) = 0;
  virtual bool isCancelled(const T &result) = 0;
  /**
   * Time to wait before re-running this task if necessary
   * @return milliseconds since epoch after which we are eligible to re-run this task.
   */
  virtual std::chrono::steady_clock::duration wait_time() = 0;
};

/**
 * Uses the wait time for a given worker to determine if it is eligible to run
 */

struct TaskRescheduleInfo {
  TaskRescheduleInfo(bool result, std::chrono::steady_clock::duration wait_time)
    : wait_time_(wait_time), finished_(result) {
    gsl_Expects(wait_time >= std::chrono::milliseconds(0));
  }

  std::chrono::steady_clock::duration wait_time_;
  bool finished_;

  static TaskRescheduleInfo Done() {
    return {true, std::chrono::steady_clock::duration(0)};
  }

  static TaskRescheduleInfo RetryIn(std::chrono::steady_clock::duration interval) {
    return {false, interval};
  }

  static TaskRescheduleInfo RetryAfter(std::chrono::steady_clock::time_point time_point) {
    auto interval = std::max(time_point - std::chrono::steady_clock::now(), std::chrono::steady_clock::duration(0));
    return {false, interval};
  }

  static TaskRescheduleInfo RetryImmediately() {
    return {false, std::chrono::steady_clock::duration(0)};
  }

#if defined(WIN32)
// https://developercommunity.visualstudio.com/content/problem/60897/c-shared-state-futuresstate-default-constructs-the.html
// Because of this bug we need to have this object default constructible, which makes no sense otherwise. Hack.
 private:
  TaskRescheduleInfo() : wait_time_(std::chrono::steady_clock::duration(0)), finished_(true) {}
  friend class std::_Associated_state<TaskRescheduleInfo>;
#endif
};

class ComplexMonitor : public utils::AfterExecute<TaskRescheduleInfo> {
 public:
  ComplexMonitor() = default;

  bool isFinished(const TaskRescheduleInfo &result) override {
    if (result.finished_) {
      return true;
    }
    current_wait_.store(result.wait_time_);
    return false;
  }
  bool isCancelled(const TaskRescheduleInfo& /*result*/) override {
    return false;
  }

  std::chrono::steady_clock::duration wait_time() override {
    return current_wait_.load();
  }

 private:
  std::atomic<std::chrono::steady_clock::duration> current_wait_ {std::chrono::steady_clock::duration(0)};
};

}  // namespace org::apache::nifi::minifi::utils
