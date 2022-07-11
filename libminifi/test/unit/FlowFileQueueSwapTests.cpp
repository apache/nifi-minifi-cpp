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

#include <random>

#include "Connection.h"

#include "../TestBase.h"
#include "SwapTestController.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Setting swap threshold sets underlying queue limits", "[SwapTest1]") {
  const size_t target_size = 4;
  const size_t min_size = target_size / 2;
  const size_t max_size = target_size * 3 / 2;

  minifi::Connection conn(nullptr, nullptr, "");
  conn.setSwapThreshold(target_size);
  REQUIRE(utils::FlowFileQueueTestAccessor::get_min_size_(utils::ConnectionTestAccessor::get_queue_(conn)) == min_size);
  REQUIRE(utils::FlowFileQueueTestAccessor::get_target_size_(utils::ConnectionTestAccessor::get_queue_(conn)) == target_size);
  REQUIRE(utils::FlowFileQueueTestAccessor::get_max_size_(utils::ConnectionTestAccessor::get_queue_(conn)) == max_size);
}

TEST_CASE_METHOD(SwapTestController, "Default constructed FlowFileQueue won't swap", "[SwapTest2]") {
  for (unsigned i = 0; i < 100; ++i) {
    pushAll({i});
  }

  REQUIRE(queue_->impl.size() == 100);

  clock_->advance(std::chrono::seconds{200});
  for (size_t i = 0; i < 100; ++i) {
    queue_->poll();
  }

  REQUIRE(queue_->impl.empty());
  verifySwapEvents({});
}

TEST_CASE_METHOD(SwapTestController, "Up to max no swap-out is triggered", "[SwapTest3]") {
  setLimits(2, 4, 6);
  pushAll({50, 20, 30, 60, 10, 40});

  REQUIRE_FALSE(queue_->isWorkAvailable());
  verifySwapEvents({});
}

TEST_CASE_METHOD(SwapTestController, "Pushing beyond max triggers a swap-out", "[SwapTest4]") {
  setLimits(2, 4, 6);
  pushAll({50, 20, 30, 60, 10, 40});

  pushAll({28});
  // size goes from 7 to 4, 3 largest must have been swapped out
  verifySwapEvents({{Store, {60, 50, 40}}});
  verifyQueue({10, 20, 28, 30}, {}, {40, 50, 60});
}

TEST_CASE_METHOD(SwapTestController, "Popping until min size does not trigger swap-in", "[SwapTest5]") {
  setLimits(2, 4, 6);
  pushAll({50, 20, 30, 60, 10, 40, 28});
  clearSwapEvents();
  clock_->advance(std::chrono::seconds{35});
  REQUIRE(queue_->isWorkAvailable());
  popAll({10, 20});
  verifyQueue({28, 30}, {}, {40, 50, 60});
  verifySwapEvents({});
}

TEST_CASE_METHOD(SwapTestController, "Popping beyond min size triggers swap-in", "[SwapTest6]") {
  setLimits(2, 4, 6);
  pushAll({50, 20, 30, 60, 10, 40, 28});
  clearSwapEvents();
  clock_->advance(std::chrono::seconds{35});
  popAll({10, 20, 28});

  // trying to swap-in all three swapped flow files
  verifyQueue({30}, {{}}, {});
  verifySwapEvents({{Load, {40, 50, 60}}});
}

TEST_CASE_METHOD(SwapTestController, "Pushing while a swap-in is pending", "[SwapTest7]") {
  setLimits(2, 4, 6);
  pushAll({50, 20, 30, 60, 10, 40, 28});
  clock_->advance(std::chrono::seconds{35});
  popAll({10, 20, 28});
  verifyQueue({30}, {{}}, {});
  clearSwapEvents();

  SECTION("Pushing into the pending swap-in range") {
    pushAll({45});
    verifyQueue({30}, {{45}}, {});
    verifySwapEvents({});
  }

  SECTION("Pushing before the pending swap-in range") {
    pushAll({35});
    verifyQueue({30, 35}, {{}}, {});
    verifySwapEvents({});
  }

  SECTION("Pushing after the pending swap-in range") {
    pushAll({65});
    verifyQueue({30}, {{}}, {65});
    verifySwapEvents({{Store, {65}}});
  }
}

TEST_CASE_METHOD(SwapTestController, "isWorkAvailable depends on the swap-in task", "[SwapTest8]") {
  setLimits(2, 4, 6);
  pushAll({50, 20, 30, 60, 10, 40, 28});
  popAll({10, 20, 28});
  verifyQueue({30}, {{}}, {});

  REQUIRE_FALSE(queue_->isWorkAvailable());

  clock_->advance(std::chrono::seconds{35});

  REQUIRE(queue_->isWorkAvailable());
  popAll({30});
  REQUIRE_FALSE(queue_->isWorkAvailable());

  SECTION("Load is completed but the minimum of those files is still not viable") {
    flow_repo_->load_tasks_[0].complete();
    REQUIRE_FALSE(queue_->isWorkAvailable());
  }

  SECTION("The minimum of the load task is viable but not yet completed") {
    clock_->advance(std::chrono::seconds{35});
    REQUIRE_FALSE(queue_->isWorkAvailable());
    // completing the task renders it viable
    flow_repo_->load_tasks_[0].complete();
    REQUIRE(queue_->isWorkAvailable());
  }
}

TEST_CASE_METHOD(SwapTestController, "Polling from load task", "[SwapTest8]") {
  setLimits(2, 4, 6);
  pushAll({50, 20, 30, 60, 10, 40, 28});
  popAll({10, 20, 28});
  pushAll({45});
  verifyQueue({30}, {{45}}, {});

  flow_repo_->load_tasks_[0].complete();

  clock_->advance(std::chrono::seconds{100});

  popAll({30, 40, 45, 50, 60}, true);
  verifyQueue({}, {}, {});
}

TEST_CASE_METHOD(SwapTestController, "Popping below min checks if the pending load is finished", "[SwapTest8]") {
  setLimits(6, 8, 10);
  pushAll({10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120});
  verifyQueue({10, 20, 30, 40, 50, 60, 70, 80}, {}, {90, 100, 110, 120});
  clock_->advance(std::chrono::seconds{200});
  clearSwapEvents();
  popAll({10, 20, 30});
  verifySwapEvents({{Load, {90, 100, 110}}});
  verifyQueue({40, 50, 60, 70, 80}, {{}}, {120});
  clearSwapEvents();

  popAll({40, 50});
  verifyQueue({60, 70, 80}, {{}}, {120});
  flow_repo_->load_tasks_[0].complete();
  popAll({60});
  // even though the live queue is not empty we check if
  // the load_task is finished and initiate a load if need be
  verifySwapEvents({{Load, {120}}});
  verifyQueue({70, 80, 90, 100, 110}, {{}}, {});
}

}  // namespace org::apache::nifi::minifi::test
