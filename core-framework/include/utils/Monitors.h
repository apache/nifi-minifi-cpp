// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
