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
#if defined(WIN32)
#include <future>  // This is required to work around a VS2017 bug, see the details below
#endif

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


struct ComplexTaskResult {
  ComplexTaskResult(bool result, std::chrono::milliseconds wait_time)
    : finished_(result), wait_time_(wait_time){}
  std::chrono::milliseconds wait_time_;
  bool finished_;

  static ComplexTaskResult Done() {
    return ComplexTaskResult(true, std::chrono::milliseconds(0));
  }

  static ComplexTaskResult RetryIn(std::chrono::milliseconds interval) {
    return ComplexTaskResult(false, interval);
  }

  static ComplexTaskResult RetryImmediately() {
    return ComplexTaskResult(false, std::chrono::milliseconds(0));
  }

#if defined(WIN32)
 // https://developercommunity.visualstudio.com/content/problem/60897/c-shared-state-futuresstate-default-constructs-the.html
 // Because of this bug we need to have this object default constructible, which makes no sense otherwise. Hack.
 private:
  ComplexTaskResult() : wait_time_(std::chrono::milliseconds(0)), finished_(true) {}
  friend class std::_Associated_state<ComplexTaskResult>;
#endif
};

class ComplexMonitor : public utils::AfterExecute<ComplexTaskResult> {
 public:
  ComplexMonitor() = default;

  virtual bool isFinished(const ComplexTaskResult &result) override {
    if (result.finished_) {
      return true;
    }
    current_wait_.store(result.wait_time_);
    return false;
  }
  virtual bool isCancelled(const ComplexTaskResult &result) override {
    return false;
  }
  /**
   * Time to wait before re-running this task if necessary
   * @return milliseconds since epoch after which we are eligible to re-run this task.
   */
  virtual std::chrono::milliseconds wait_time() override {
    return current_wait_.load();
  }

 private:
  std::atomic<std::chrono::milliseconds> current_wait_ {std::chrono::milliseconds(0)};
};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_MONITORS_H