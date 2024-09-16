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

namespace org::apache::nifi::minifi::utils {

class TaskRescheduleInfo {
 public:
  TaskRescheduleInfo(bool result, std::chrono::steady_clock::time_point next_execution_time)
    : next_execution_time_(next_execution_time), finished_(result) {}

  static TaskRescheduleInfo Done() {
    return {true, std::chrono::steady_clock::time_point::min()};
  }

  static TaskRescheduleInfo RetryAfter(std::chrono::steady_clock::time_point next_execution_time) {
    return {false, next_execution_time};
  }

  static TaskRescheduleInfo RetryIn(std::chrono::steady_clock::duration duration) {
    return {false, std::chrono::steady_clock::now()+duration};
  }

  static TaskRescheduleInfo RetryImmediately() {
    return {false, std::chrono::steady_clock::time_point::min()};
  }

  [[nodiscard]] std::chrono::steady_clock::time_point getNextExecutionTime() const {
    return next_execution_time_;
  }

  [[nodiscard]] bool isFinished() const {
    return finished_;
  }

 private:
  std::chrono::steady_clock::time_point next_execution_time_;
  bool finished_;
};


}  // namespace org::apache::nifi::minifi::utils
