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

#ifndef NIFI_MINIFI_CPP_MONITORS_H
#define NIFI_MINIFI_CPP_MONITORS_H

#include <chrono>
#include <atomic>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * Worker task helper that determines
 * whether or not we will run
 */
template<typename T>
class AfterExecute {
 public:
  virtual ~AfterExecute() {

  }

  explicit AfterExecute() {

  }

  explicit AfterExecute(AfterExecute &&other) {

  }
  virtual bool isFinished(const T &result) = 0;
  virtual bool isCancelled(const T &result) = 0;
  /**
   * Time to wait before re-running this task if necessary
   * @return milliseconds since epoch after which we are eligible to re-run this task.
   */
  virtual std::chrono::milliseconds wait_time() = 0;
};

/**
 * Uses the wait time for a given worker to determine if it is eligible to run
 */
class TimerAwareMonitor : public utils::AfterExecute<std::chrono::milliseconds> {
 public:
  TimerAwareMonitor(std::atomic<bool> *run_monitor)
      : current_wait_(std::chrono::milliseconds(0)),
        run_monitor_(run_monitor) {
  }
  explicit TimerAwareMonitor(TimerAwareMonitor &&other) = default;
  virtual bool isFinished(const std::chrono::milliseconds &result) override {
    current_wait_.store(result);
    if (*run_monitor_) {
      return false;
    }
    return true;
  }
  virtual bool isCancelled(const std::chrono::milliseconds &result) override {
    if (*run_monitor_) {
      return false;
    }
    return true;
  }
  /**
   * Time to wait before re-running this task if necessary
   * @return milliseconds since epoch after which we are eligible to re-run this task.
   */
  virtual std::chrono::milliseconds wait_time() override {
    return current_wait_.load();
  }
 protected:

  std::atomic<std::chrono::milliseconds> current_wait_;
  std::atomic<bool> *run_monitor_;
};

class SingleRunMonitor : public utils::AfterExecute<bool>{
 public:
  SingleRunMonitor(std::chrono::milliseconds retry_interval = std::chrono::milliseconds(100))
      : retry_interval_(retry_interval) {
  }
  explicit SingleRunMonitor(SingleRunMonitor &&other) = default;

  virtual bool isFinished(const bool &result) override {
    return result;
  }
  virtual bool isCancelled(const bool &result) override {
    return false;
  }
  virtual std::chrono::milliseconds wait_time() override {
    return retry_interval_;
  }
 protected:
  const std::chrono::milliseconds retry_interval_;
};


struct ComplexResult {
  ComplexResult(bool result, std::chrono::milliseconds wait_time)
    : finished_(result), wait_time_(wait_time){}
  std::chrono::milliseconds wait_time_;
  bool finished_;
};

class ComplexMonitor : public utils::AfterExecute<ComplexResult> {
 public:
  ComplexMonitor(std::atomic<bool> *run_monitor)
  : current_wait_(std::chrono::milliseconds(0)),
    run_monitor_(run_monitor) {
  }
  explicit ComplexMonitor(ComplexMonitor &&other) = default;

  virtual bool isFinished(const ComplexResult &result) override {
    if (result.finished_) {
      return true;
    }
    if (*run_monitor_) {
      current_wait_.store(result.wait_time_);
      return false;
    }
    return true;
  }
  virtual bool isCancelled(const ComplexResult &result) override {
    if (*run_monitor_) {
      return false;
    }
    return true;
  }
  /**
   * Time to wait before re-running this task if necessary
   * @return milliseconds since epoch after which we are eligible to re-run this task.
   */
  virtual std::chrono::milliseconds wait_time() override {
    return current_wait_.load();
  }

 private:
  std::atomic<std::chrono::milliseconds> current_wait_;
  std::atomic<bool> *run_monitor_;
};

static ComplexResult Done() {
  return ComplexResult(true, std::chrono::milliseconds(0));
}

static ComplexResult Retry(std::chrono::milliseconds interval) {
  return ComplexResult(false, interval);
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_MONITORS_H