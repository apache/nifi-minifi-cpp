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

#include <chrono>
#include "FlowFileQueue.h"

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"

namespace core = minifi::core;

TEST_CASE("After construction, a FlowFileQueue is empty", "[FlowFileQueue]") {
  utils::FlowFileQueue queue;

  REQUIRE(queue.empty());
  REQUIRE(queue.size() == 0);  // NOLINT(readability-container-size-empty)
  REQUIRE_FALSE(queue.isWorkAvailable());
  REQUIRE_THROWS(queue.pop());
}

TEST_CASE("If a non-penalized flow file is added to the FlowFileQueue, we can pop it", "[FlowFileQueue][pop]") {
  utils::FlowFileQueue queue;
  const auto flow_file = std::make_shared<core::FlowFileImpl>();
  queue.push(flow_file);

  REQUIRE_FALSE(queue.empty());
  REQUIRE(queue.size() == 1);
  REQUIRE(queue.isWorkAvailable());
  REQUIRE(queue.pop() == flow_file);
}

TEST_CASE("A flow file can be moved into the FlowFileQueue", "[FlowFileQueue][pop]") {
  utils::FlowFileQueue queue;

  auto penalized_flow_file = std::make_shared<core::FlowFileImpl>();
  penalized_flow_file->penalize(std::chrono::milliseconds{100});
  queue.push(std::move(penalized_flow_file));

  queue.push(std::make_shared<core::FlowFileImpl>());

  REQUIRE_FALSE(queue.empty());
  REQUIRE(queue.size() == 2);
}

TEST_CASE("If three flow files are added to the FlowFileQueue, we can pop them in FIFO order", "[FlowFileQueue][pop]") {
  utils::FlowFileQueue queue;
  const auto flow_file_1 = std::make_shared<core::FlowFileImpl>();
  queue.push(flow_file_1);
  const auto flow_file_2 = std::make_shared<core::FlowFileImpl>();
  queue.push(flow_file_2);
  const auto flow_file_3 = std::make_shared<core::FlowFileImpl>();
  queue.push(flow_file_3);

  REQUIRE(queue.isWorkAvailable());
  REQUIRE(queue.pop() == flow_file_1);
  REQUIRE(queue.isWorkAvailable());
  REQUIRE(queue.pop() == flow_file_2);
  REQUIRE(queue.isWorkAvailable());
  REQUIRE(queue.pop() == flow_file_3);
  REQUIRE_FALSE(queue.isWorkAvailable());
}

TEST_CASE("Cannot add flow files in the past preempting others", "[FlowFileQueue][pop]") {
  utils::FlowFileQueue queue;
  const auto flow_file_1 = std::make_shared<core::FlowFileImpl>();
  queue.push(flow_file_1);
  const auto flow_file_2 = std::make_shared<core::FlowFileImpl>();
  flow_file_2->penalize(std::chrono::seconds{-10});
  queue.push(flow_file_2);

  REQUIRE(queue.isWorkAvailable());
  REQUIRE(queue.pop() == flow_file_1);
  REQUIRE(queue.isWorkAvailable());
  REQUIRE(queue.pop() == flow_file_2);
  REQUIRE_FALSE(queue.isWorkAvailable());
}

namespace {

class PenaltyHasExpired {
 public:
  explicit PenaltyHasExpired(std::shared_ptr<core::FlowFile> flow_file) : flow_file_(std::move(flow_file)) {}
  bool operator()() { return !flow_file_->isPenalized(); }

 private:
  std::shared_ptr<core::FlowFile> flow_file_;
};

}  // namespace

TEST_CASE("Penalized flow files are popped from the FlowFileQueue in the order their penalties expire", "[FlowFileQueue][pop]") {
  utils::FlowFileQueue queue;
  const auto flow_file_1 = std::make_shared<core::FlowFileImpl>();
  flow_file_1->penalize(std::chrono::milliseconds{70});
  queue.push(flow_file_1);
  const auto flow_file_2 = std::make_shared<core::FlowFileImpl>();
  flow_file_2->penalize(std::chrono::milliseconds{50});
  queue.push(flow_file_2);
  const auto flow_file_3 = std::make_shared<core::FlowFileImpl>();
  flow_file_3->penalize(std::chrono::milliseconds{80});
  queue.push(flow_file_3);
  const auto flow_file_4 = std::make_shared<core::FlowFileImpl>();
  flow_file_4->penalize(std::chrono::milliseconds{60});
  queue.push(flow_file_4);

  REQUIRE_FALSE(queue.isWorkAvailable());

  REQUIRE(minifi::test::utils::verifyEventHappenedInPollTime(std::chrono::seconds{1}, PenaltyHasExpired{flow_file_2}, std::chrono::milliseconds{10}));
  REQUIRE(queue.isWorkAvailable());
  REQUIRE(queue.pop() == flow_file_2);

  REQUIRE(minifi::test::utils::verifyEventHappenedInPollTime(std::chrono::seconds{1}, PenaltyHasExpired{flow_file_4}, std::chrono::milliseconds{10}));
  REQUIRE(queue.isWorkAvailable());
  REQUIRE(queue.pop() == flow_file_4);

  REQUIRE(minifi::test::utils::verifyEventHappenedInPollTime(std::chrono::seconds{1}, PenaltyHasExpired{flow_file_1}, std::chrono::milliseconds{10}));
  REQUIRE(queue.isWorkAvailable());
  REQUIRE(queue.pop() == flow_file_1);

  REQUIRE(minifi::test::utils::verifyEventHappenedInPollTime(std::chrono::seconds{1}, PenaltyHasExpired{flow_file_3}, std::chrono::milliseconds{10}));
  REQUIRE(queue.isWorkAvailable());
  REQUIRE(queue.pop() == flow_file_3);

  REQUIRE_FALSE(queue.isWorkAvailable());
}

TEST_CASE("If a penalized then a non-penalized flow file is added to the FlowFileQueue, pop() returns the correct one", "[FlowFileQueue][pop]") {
  utils::FlowFileQueue queue;
  const auto penalized_flow_file = std::make_shared<core::FlowFileImpl>();
  penalized_flow_file->penalize(std::chrono::milliseconds{10});
  queue.push(penalized_flow_file);
  const auto flow_file = std::make_shared<core::FlowFileImpl>();
  queue.push(flow_file);

  SECTION("Try popping right away") {
    REQUIRE(queue.isWorkAvailable());
    REQUIRE(queue.pop() == flow_file);
    REQUIRE_FALSE(queue.isWorkAvailable());
  }

  SECTION("Wait until the penalty expires, then pop") {
    REQUIRE(minifi::test::utils::verifyEventHappenedInPollTime(std::chrono::seconds{1}, PenaltyHasExpired{penalized_flow_file}, std::chrono::milliseconds{10}));

    REQUIRE(queue.isWorkAvailable());
    REQUIRE(queue.pop() == flow_file);
    REQUIRE(queue.isWorkAvailable());
    REQUIRE(queue.pop() == penalized_flow_file);
    REQUIRE_FALSE(queue.isWorkAvailable());
  }
}

TEST_CASE("Force pop on FlowFileQueue returns the flow files, whether penalized or not", "[FlowFileQueue][forcePop]") {
  utils::FlowFileQueue queue;
  const auto penalized_flow_file = std::make_shared<core::FlowFileImpl>();
  penalized_flow_file->penalize(std::chrono::milliseconds{10});
  queue.push(penalized_flow_file);
  const auto flow_file = std::make_shared<core::FlowFileImpl>();
  queue.push(flow_file);

  REQUIRE_FALSE(queue.empty());
  REQUIRE(queue.pop() == flow_file);
  REQUIRE(queue.pop() == penalized_flow_file);
  REQUIRE(queue.empty());
}
