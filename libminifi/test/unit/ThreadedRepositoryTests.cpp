// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <algorithm>
#include <atomic>
#include <string>
#include <thread>

#include "core/ThreadedRepository.h"
#include "core/repository/NoOpThreadedRepository.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"
#include "utils/BackTrace.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core::test {

using BackTrace = utils::trace_resolver::BackTrace;

class BlockingThreadedRepository : public core::ThreadedRepositoryImpl {
 public:
  BlockingThreadedRepository() : ThreadedRepositoryImpl("BlockingRepo") {}

  BlockingThreadedRepository(BlockingThreadedRepository&&) = delete;
  BlockingThreadedRepository(const BlockingThreadedRepository&) = delete;
  BlockingThreadedRepository& operator=(BlockingThreadedRepository&&) = delete;
  BlockingThreadedRepository& operator=(const BlockingThreadedRepository&) = delete;

  ~BlockingThreadedRepository() override {
    release();
    stop();
  }

  uint64_t getRepositorySize() const override {
    return 0;
  }

  uint64_t getRepositoryEntryCount() const override {
    return 0;
  }

  void waitUntilRunIsInProgress() {
    run_in_progress_.wait(false);
  }

  void release() {
    release_.test_and_set();
    release_.notify_all();
  }

 private:
  void run() override {
    run_in_progress_.test_and_set();
    run_in_progress_.notify_all();
    release_.wait(false);
  }

  std::thread& getThread() override {
    return thread_;
  }

  std::atomic_flag run_in_progress_;
  std::atomic_flag release_;
  std::thread thread_;
};

TEST_CASE("ThreadedRepository returns the stack trace of its thread while it is running", "[ThreadedRepository][getTraces]") {
  LogTestController::getInstance().setTrace<BackTrace>();

  BlockingThreadedRepository repository;

  SECTION("before start") {
    const BackTrace back_trace = repository.getTraces();
    CHECK(back_trace.name == "BlockingRepo");
    CHECK(back_trace.trace.empty());
  }

  SECTION("while running") {
    REQUIRE(repository.start());
    repository.waitUntilRunIsInProgress();

    const BackTrace back_trace = repository.getTraces();
    CHECK(back_trace.name == "BlockingRepo");
#ifdef HAS_EXECINFO
    CHECK_FALSE(back_trace.trace.empty());
#endif
  }

  SECTION("after stop") {
    REQUIRE(repository.start());
    repository.waitUntilRunIsInProgress();
    repository.release();
    REQUIRE(repository.stop());

    const BackTrace back_trace = repository.getTraces();
    CHECK(back_trace.name == "BlockingRepo");
    CHECK(back_trace.trace.empty());
  }
}

TEST_CASE("ThreadedRepository returns an empty stack trace if run() has finished", "[ThreadedRepository][getTraces]") {
  LogTestController::getInstance().setTrace<BackTrace>();

  core::repository::NoOpThreadedRepository repository{"NoOpRepo"};
  REQUIRE(repository.start());
  CHECK(repository.isRunning());

  // the thread exits as soon as it starts, as run() is empty; but if it hasn't exited yet, we'll get a non-empty trace
  std::this_thread::sleep_for(100ms);
  REQUIRE(minifi::test::utils::verifyEventHappenedInPollTime(1s, [&] {
    const BackTrace back_trace = repository.getTraces();
    return back_trace.name == "NoOpRepo" && back_trace.trace.empty();
  }));
  CHECK_FALSE(LogTestController::getInstance().contains("timed out waiting for the stack trace", 0s));

  CHECK(repository.isRunning());
  CHECK(repository.stop());
  CHECK_FALSE(repository.isRunning());
}

}  // namespace org::apache::nifi::minifi::core::test
