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

TEST_CASE("ThreadPoolTest1", "[TPT1]") {
  utils::ThreadPool pool(5);
  utils::Worker functor([](){ return utils::TaskRescheduleInfo::Done(); }, "id");
  pool.start();
  std::future<utils::TaskRescheduleInfo> fut;
  pool.execute(std::move(functor), fut);  // NOLINT(bugprone-use-after-move)
  CHECK(fut.get().isFinished());
}

TEST_CASE("ThreadPoolTest2", "[TPT2]") {
  constexpr size_t max_counter = 20;
  std::atomic<size_t> counter = 0;
  utils::ThreadPool pool(5);
  utils::Worker functor([&counter](){
    if (++counter == max_counter)
      return utils::TaskRescheduleInfo::Done();

    return utils::TaskRescheduleInfo::RetryImmediately();}, "id");
  pool.start();
  std::future<utils::TaskRescheduleInfo> fut;
  pool.execute(std::move(functor), fut);  // NOLINT(bugprone-use-after-move)
  CHECK(fut.get().isFinished());
  REQUIRE(20 == counter);
}

TEST_CASE("Worker wait time should be relative to the last run") {
  std::vector<std::chrono::steady_clock::time_point> worker_execution_time_points;
  utils::ThreadPool pool(1);
  auto wait_time_between_tasks = 10ms;
  utils::Worker worker([&]()->utils::TaskRescheduleInfo {
    worker_execution_time_points.push_back(std::chrono::steady_clock::now());
    if (worker_execution_time_points.size() == 2) {
      return utils::TaskRescheduleInfo::Done();
    } else {
      return utils::TaskRescheduleInfo::RetryIn(wait_time_between_tasks);
    }
  }, "id");
  std::this_thread::sleep_for(wait_time_between_tasks + 1ms);  // Pre-waiting should not matter

  std::future<utils::TaskRescheduleInfo> task_future;
  pool.execute(std::move(worker), task_future);
  pool.start();

  auto final_task_reschedule_info = task_future.get();

  CHECK(final_task_reschedule_info.isFinished());
  REQUIRE(worker_execution_time_points.size() == 2);
  CHECK(worker_execution_time_points[1] - worker_execution_time_points[0] >= wait_time_between_tasks);
}
