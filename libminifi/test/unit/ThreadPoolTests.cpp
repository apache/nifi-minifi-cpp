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

#include <utility>
#include <future>
#include <memory>
#include "../TestBase.h"
#include "../Catch.h"
#include "utils/ThreadPool.h"

using namespace std::literals::chrono_literals;

bool function() {
  return true;
}

class WorkerNumberExecutions : public utils::AfterExecute<int> {
 public:
  explicit WorkerNumberExecutions(int tasks)
      : tasks(tasks) {
  }

  explicit WorkerNumberExecutions(WorkerNumberExecutions && other) noexcept
      : runs(other.runs),
        tasks(other.tasks) {
  }

  bool isFinished(const int &result) override {
    return result <= 0 || ++runs >= tasks;
  }
  bool isCancelled(const int& /*result*/) override {
    return false;
  }

  std::chrono::milliseconds wait_time() override {
    return 50ms;
  }

 protected:
  int runs{0};
  int tasks;
};

TEST_CASE("ThreadPoolTest1", "[TPT1]") {
  utils::ThreadPool<bool> pool(5);
  std::function<bool()> f_ex = function;
  utils::Worker<bool> functor(f_ex, "id");
  pool.start();
  std::future<bool> fut;
  pool.execute(std::move(functor), fut);  // NOLINT(bugprone-use-after-move)
  fut.wait();
  REQUIRE(true == fut.get());
}

std::atomic<int> counter;

int counterFunction() {
  return ++counter;
}

TEST_CASE("ThreadPoolTest2", "[TPT2]") {
  counter = 0;
  utils::ThreadPool<int> pool(5);
  std::function<int()> f_ex = counterFunction;
  std::unique_ptr<utils::AfterExecute<int>> after_execute = std::unique_ptr<utils::AfterExecute<int>>(new WorkerNumberExecutions(20));
  utils::Worker<int> functor(f_ex, "id", std::move(after_execute));
  pool.start();
  std::future<int> fut;
  pool.execute(std::move(functor), fut);
  fut.wait();
  REQUIRE(20 == fut.get());
}

TEST_CASE("Worker wait time should be relative to the last run") {
  std::mutex cv_m;
  std::condition_variable cv;
  std::vector<std::chrono::steady_clock::time_point> worker_execution_time_points;
  utils::ThreadPool<int> pool(1);
  std::unique_ptr<utils::AfterExecute<int>> after_execute = std::unique_ptr<utils::AfterExecute<int>>(new WorkerNumberExecutions(100));
  utils::Worker<int> worker([&]()->int {
    {
      std::unique_lock lock(cv_m);
      worker_execution_time_points.push_back(std::chrono::steady_clock::now());
    }
    cv.notify_all();
    return 1;
  }, "id", std::move(after_execute));
  std::this_thread::sleep_for(60ms);  // Pre-waiting should not matter

  std::future<int> irrelevant_future;
  pool.execute(std::move(worker), irrelevant_future);
  pool.start();

  std::unique_lock lock(cv_m);
  cv.wait(lock, [&]() {return worker_execution_time_points.size() >= 2;});
  REQUIRE(worker_execution_time_points.size() >= 2);
  CHECK(worker_execution_time_points[1] - worker_execution_time_points[0] >= 50ms);
}
