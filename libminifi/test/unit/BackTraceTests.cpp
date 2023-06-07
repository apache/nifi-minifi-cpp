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
#include "utils/BackTrace.h"
#include "utils/Monitors.h"
#include "utils/ThreadPool.h"

using namespace std::literals::chrono_literals;

class WorkerNumberExecutions : public utils::AfterExecute<int> {
 public:
  explicit WorkerNumberExecutions(int tasks)
      : tasks(tasks) {
  }

  WorkerNumberExecutions(WorkerNumberExecutions && other) noexcept
      : runs(other.runs),
      tasks(other.tasks) {
  }

  bool isFinished(const int &result) override {
    return result <= 0 || ++runs >= tasks;
  }
  bool isCancelled(const int& /*result*/) override {
    return false;
  }

  std::chrono::steady_clock::duration wait_time() override {
    return 50ms;
  }

 protected:
  int runs{0};
  int tasks;
};

TEST_CASE("BT1", "[TPT1]") {
  const BackTrace trace = TraceResolver::getResolver().getBackTrace("BT1");
#ifdef HAS_EXECINFO
  REQUIRE(!trace.getTraces().empty());
#endif
}

TEST_CASE("BT2", "[TPT2]") {
  std::atomic<int> counter = 0;
  utils::ThreadPool<int> pool(4);
  pool.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  for (int i = 0; i < 3; i++) {
    std::unique_ptr<utils::AfterExecute<int>> after_execute = std::unique_ptr<utils::AfterExecute<int>>(new WorkerNumberExecutions(5));
    utils::Worker<int> functor([&counter]() { return ++counter; }, "id", std::move(after_execute));

    std::future<int> fut;
    pool.execute(std::move(functor), fut);  // NOLINT(bugprone-use-after-move)
  }

  std::unique_ptr<utils::AfterExecute<int>> after_execute = std::unique_ptr<utils::AfterExecute<int>>(new WorkerNumberExecutions(5));
  utils::Worker<int> functor([&counter]() { return ++counter; }, "id", std::move(after_execute));

  std::future<int> fut;
  pool.execute(std::move(functor), fut);  // NOLINT(bugprone-use-after-move)

  std::vector<BackTrace> traces = pool.getTraces();
  for (const auto &trace : traces) {
    std::cerr << "Thread name: " << trace.getName() << std::endl;
    const auto &trace_strings = trace.getTraces();
#ifdef HAS_EXECINFO
    REQUIRE(trace_strings.size() > 2);
    for (const auto& trace_string : trace_strings) {
      std::cerr << " - " << trace_string << std::endl;
    }
    if (trace_strings.at(0).find("sleep_for") != std::string::npos) {
      REQUIRE(trace_strings.at(1).find("counterFunction") != std::string::npos);
    }
#endif
  }
  fut.wait();
}

