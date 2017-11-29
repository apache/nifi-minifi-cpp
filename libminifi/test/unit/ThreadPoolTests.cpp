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
#include "utils/ThreadPool.h"

bool function() {
  return true;
}

class WorkerNumberExecutions : public utils::AfterExecute<int> {
 public:
  explicit WorkerNumberExecutions(int tasks)
      : runs(0),
        tasks(tasks) {
  }

  explicit WorkerNumberExecutions(WorkerNumberExecutions && other)
      : runs(std::move(other.runs)),
        tasks(std::move(other.tasks)) {
  }

  ~WorkerNumberExecutions() {
  }

  virtual bool isFinished(const int &result) {
    if (result > 0 && ++runs < tasks) {
      return false;
    } else {
      return true;
    }
  }
  virtual bool isCancelled(const int &result) {
    return false;
  }

  int getRuns() {
    return runs;
  }

  virtual int64_t wait_time() {
    // wait 50ms
    return 50;
  }

 protected:
  int runs;
  int tasks;
};

TEST_CASE("ThreadPoolTest1", "[TPT1]") {
  utils::ThreadPool<bool> pool(5);
  std::function<bool()> f_ex = function;
  utils::Worker<bool> functor(f_ex, "id");
  pool.start();
  std::future<bool> fut;
  REQUIRE(true == pool.execute(std::move(functor), fut));
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
  REQUIRE(true == pool.execute(std::move(functor), fut));
  fut.wait();
  REQUIRE(20 == fut.get());
}
