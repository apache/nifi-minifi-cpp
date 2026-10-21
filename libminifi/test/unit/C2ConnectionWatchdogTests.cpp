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
#include <optional>
#include <utility>

#include "C2ShutdownRecorder.h"
#include "c2/C2ConnectionWatchdog.h"
#include "c2/C2ExitCodes.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

namespace {
void verifyTimeoutShutdownRequested(C2ShutdownRecorder& recorder) {
  REQUIRE(utils::verifyEventHappenedInPollTime(2s, [&] { return recorder.count() > 0; }, 10ms));
  REQUIRE(recorder.count() == 1);
  const auto [shutdown_type, exit_code] = recorder.firstCall();
  CHECK(shutdown_type == ShutdownType::SHUTDOWN);
  REQUIRE(exit_code.has_value());
  CHECK(*exit_code == c2::C2_CONNECTION_TIMEOUT_EXIT_CODE);
}
}  // namespace

TEST_CASE("Test invalid values", "[c2watchdog]") {
  LogTestController::getInstance().reset();
  SECTION("Zero timeout value") {
    REQUIRE_THROWS_WITH(c2::C2ConnectionWatchdog(0ms, [](ShutdownType, std::optional<int32_t>) {}),
        "C2 connection timeout must be greater than 0");
  }

  SECTION("Negative timeout value") {
    REQUIRE_THROWS_WITH(c2::C2ConnectionWatchdog(-1ms, [](ShutdownType, std::optional<int32_t>) {}),
        "C2 connection timeout must be greater than 0");
  }
}

TEST_CASE("Test shutdown when no C2 connection is present at all", "[c2watchdog]") {
  LogTestController::getInstance().reset();
  C2ShutdownRecorder recorder;
  c2::C2ConnectionWatchdog watchdog(400ms, recorder.callback());
  verifyTimeoutShutdownRequested(recorder);
  REQUIRE(LogTestController::getInstance().contains("C2 connection timeout exceeded, shutting down...", 1s));
}

TEST_CASE("Test shutdown when C2 connection is lost", "[c2watchdog]") {
  LogTestController::getInstance().reset();
  C2ShutdownRecorder recorder;
  c2::C2ConnectionWatchdog watchdog(400ms, recorder.callback());
  for (int i = 0; i < 10; ++i) {
    watchdog.heartBeatHappened();
    REQUIRE(recorder.count() == 0);
    std::this_thread::sleep_for(50ms);
  }
  verifyTimeoutShutdownRequested(recorder);
  REQUIRE(LogTestController::getInstance().contains("C2 connection timeout exceeded, shutting down...", 1s));
}

TEST_CASE("Shutdown is only requested once", "[c2watchdog]") {
  LogTestController::getInstance().reset();
  C2ShutdownRecorder recorder;
  c2::C2ConnectionWatchdog watchdog(100ms, recorder.callback());
  verifyTimeoutShutdownRequested(recorder);
  std::this_thread::sleep_for(300ms);
  REQUIRE(recorder.count() == 1);
}

TEST_CASE("Watchdog is destroyed promptly even with a long timeout", "[c2watchdog]") {
  LogTestController::getInstance().reset();
  C2ShutdownRecorder recorder;
  auto watchdog = std::make_unique<c2::C2ConnectionWatchdog>(1h, recorder.callback());
  const auto destruction_start = std::chrono::steady_clock::now();
  watchdog.reset();
  REQUIRE(std::chrono::steady_clock::now() - destruction_start < 1s);
  REQUIRE(recorder.count() == 0);
}

TEST_CASE("No shutdown is requested after the watchdog is destroyed", "[c2watchdog]") {
  LogTestController::getInstance().reset();
  C2ShutdownRecorder recorder;
  { c2::C2ConnectionWatchdog watchdog(200ms, recorder.callback()); }
  std::this_thread::sleep_for(500ms);
  REQUIRE(recorder.count() == 0);
}

}  // namespace org::apache::nifi::minifi::test
