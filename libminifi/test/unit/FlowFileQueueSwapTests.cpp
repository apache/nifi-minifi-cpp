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
#include <span>
#include <string>
#include <vector>
#include <utility>
#include <memory>

#include "Connection.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"
#include "unit/ProvenanceTestHelper.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::test {

using Timepoint = std::chrono::time_point<std::chrono::steady_clock>;

enum EventKind {
  Store, Load
};

struct SwapEvent {
  EventKind kind;
  std::vector<minifi::SwappedFlowFile> flow_files;

  void verifyTimes(std::initializer_list<unsigned> seconds) {
    REQUIRE(flow_files.size() == seconds.size());
    size_t idx = 0;
    for (auto& second : seconds) {
      REQUIRE(flow_files[idx].to_be_processed_after == Timepoint{std::chrono::seconds{second}});
      ++idx;
    }
  }
};

class SwappingFlowFileTestRepo : public TestFlowRepository, public minifi::SwapManager {
 public:
  void store(std::vector<std::shared_ptr<core::FlowFile>> flow_files) override {
    std::vector<minifi::SwappedFlowFile> ids;
    for (const auto& ff : flow_files) {
      ids.push_back(minifi::SwappedFlowFile{ff->getUUID(), ff->getPenaltyExpiration()});
      minifi::io::BufferStream output;
      std::dynamic_pointer_cast<minifi::FlowFileRecord>(ff)->Serialize(output);
      Put(ff->getUUIDStr().c_str(), reinterpret_cast<const uint8_t*>(output.getBuffer().data()), output.size());
    }
    swap_events_.push_back({Store, ids});
  }

  std::future<std::vector<std::shared_ptr<core::FlowFile>>> load(std::vector<minifi::SwappedFlowFile> flow_files) override {
    swap_events_.push_back({Load, flow_files});
    LoadTask load_task;
    auto future = load_task.promise.get_future();
    load_task.result.reserve(flow_files.size());
    for (const auto& ff_id : flow_files) {
      std::string value;
      Get(ff_id.id.to_string().c_str(), value);
      minifi::utils::Identifier container_id;
      auto ff = minifi::FlowFileRecord::DeSerialize(gsl::make_span(value).as_span<const std::byte>(), content_repo_, container_id);
      ff->setPenaltyExpiration(ff_id.to_be_processed_after);
      load_task.result.push_back(std::move(ff));
    }
    load_tasks_.push_back(std::move(load_task));
    return future;
  }

  struct LoadTask {
    std::promise<std::vector<std::shared_ptr<core::FlowFile>>> promise;
    std::vector<std::shared_ptr<core::FlowFile>> result;

    void complete() {
      promise.set_value(result);
    }
  };

  std::vector<LoadTask> load_tasks_;
  std::vector<SwapEvent> swap_events_;
};

using FlowFilePtr = std::shared_ptr<core::FlowFile>;
using FlowFilePtrVec = std::vector<FlowFilePtr>;

struct FlowFileComparator {
  bool operator()(const FlowFilePtr& left, const FlowFilePtr& right) const {
    return left->getPenaltyExpiration() < right->getPenaltyExpiration();
  }
};

struct VerifiedQueue {
  void push(FlowFilePtr ff) {
    size();
    impl.push(ff);
    ref_.insert(std::lower_bound(ref_.begin(), ref_.end(), ff, FlowFileComparator{}), ff);
    size();
  }

  FlowFilePtr poll() {
    size();
    FlowFilePtr ff = impl.pop();
    REQUIRE(!ref_.empty());
    // the order when flow files have the same penalty is not fixed
    REQUIRE(ff->getPenaltyExpiration() == ref_.front()->getPenaltyExpiration());
    ref_.erase(ref_.begin());
    size();
    return ff;
  }

  void verify(std::initializer_list<unsigned> live, std::optional<std::initializer_list<unsigned>> inter, std::initializer_list<unsigned> swapped) const {
    // check live ffs
    auto live_copy = utils::FlowFileQueueTestAccessor::get_queue_(impl);
    REQUIRE(live_copy.size() == live.size());
    for (auto sec : live) {
      auto min = live_copy.popMin();
      REQUIRE(min->getPenaltyExpiration() == Timepoint{std::chrono::seconds{sec}});
    }

    // check inter ffs
    if (!inter) {
      REQUIRE_FALSE(utils::FlowFileQueueTestAccessor::get_load_task_(impl).has_value());
    } else {
      auto& intermediate = utils::FlowFileQueueTestAccessor::get_load_task_(impl)->intermediate_items;
      REQUIRE(intermediate.size() == inter->size());
      size_t idx = 0;
      for (auto sec : inter.value()) {
        REQUIRE(intermediate[idx]->getPenaltyExpiration() == Timepoint{std::chrono::seconds{sec}});
        ++idx;
      }
    }

    // check swapped ffs
    auto swapped_copy = utils::FlowFileQueueTestAccessor::get_swapped_flow_files_(impl);
    REQUIRE(swapped_copy.size() == swapped.size());
    for (auto sec : swapped) {
      auto min = swapped_copy.popMin();
      REQUIRE(min.to_be_processed_after == Timepoint{std::chrono::seconds{sec}});
    }
  }

  bool isWorkAvailable() const {
    return impl.isWorkAvailable();
  }

  size_t size() const {
    size_t result = impl.size();
    REQUIRE(result == ref_.size());
    return result;
  }

  explicit VerifiedQueue(std::shared_ptr<minifi::SwapManager> swap_manager)
    : impl(std::move(swap_manager)) {}

  minifi::utils::FlowFileQueue impl;
  FlowFilePtrVec ref_;
};

class SwapTestController : public TestController {
 public:
  SwapTestController() {
    content_repo_ = std::make_shared<core::repository::VolatileContentRepository>();
    flow_repo_ = std::make_shared<SwappingFlowFileTestRepo>();
    flow_repo_->loadComponent(content_repo_);
    clock_ = std::make_shared<minifi::test::utils::ManualClock>();
    minifi::utils::timeutils::setClock(clock_);
    queue_ = std::make_shared<VerifiedQueue>(std::static_pointer_cast<minifi::SwapManager>(flow_repo_));
  }

  void setLimits(size_t min_size, size_t target_size, size_t max_size) {
    queue_->impl.setMinSize(min_size);
    queue_->impl.setTargetSize(target_size);
    queue_->impl.setMaxSize(max_size);
  }

  struct SwapEventPattern {
    EventKind kind;
    std::initializer_list<unsigned > seconds;
  };

  void verifySwapEvents(std::vector<SwapEventPattern> events) {
    REQUIRE(flow_repo_->swap_events_.size() == events.size());
    size_t idx = 0;
    for (auto& pattern : events) {
      REQUIRE(pattern.kind == flow_repo_->swap_events_[idx].kind);
      flow_repo_->swap_events_[idx].verifyTimes(pattern.seconds);
    }
  }

  void clearSwapEvents() {
    flow_repo_->swap_events_.clear();
  }

  void verifyQueue(std::initializer_list<unsigned> live, std::optional<std::initializer_list<unsigned>> inter, std::initializer_list<unsigned> swapped) {
    queue_->verify(live, inter, swapped);
  }

  void pushAll(std::initializer_list<unsigned> seconds) {
    for (auto sec : seconds) {
      auto ff = std::static_pointer_cast<core::FlowFile>(std::make_shared<minifi::FlowFileRecordImpl>());
      ff->setPenaltyExpiration(Timepoint{std::chrono::seconds{sec}});
      queue_->push(std::move(ff));
    }
  }

  void popAll(std::initializer_list<unsigned> seconds, bool check_is_work_available = false) {
    for (auto sec : seconds) {
      if (check_is_work_available) {
        REQUIRE(queue_->isWorkAvailable());
      }
      auto ff = queue_->poll();
      REQUIRE(ff->getPenaltyExpiration() == Timepoint{std::chrono::seconds{sec}});
    }
  }

  std::shared_ptr<SwappingFlowFileTestRepo> flow_repo_;
  std::shared_ptr<core::repository::VolatileContentRepository> content_repo_;
  std::shared_ptr<VerifiedQueue> queue_;
  std::shared_ptr<minifi::test::utils::ManualClock> clock_;
};

TEST_CASE("Setting swap threshold sets underlying queue limits", "[SwapTest1]") {
  const size_t target_size = 4;
  const size_t min_size = target_size / 2;
  const size_t max_size = target_size * 3 / 2;

  minifi::ConnectionImpl conn(nullptr, nullptr, "");
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
