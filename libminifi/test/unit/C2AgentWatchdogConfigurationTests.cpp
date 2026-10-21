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
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "C2ShutdownRecorder.h"
#include "c2/C2Agent.h"
#include "c2/C2ExitCodes.h"
#include "properties/Configure.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "utils/file/FileSystem.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class C2AgentWatchdogConfigurationTestFixture {
 public:
  C2AgentWatchdogConfigurationTestFixture() {
    LogTestController::getInstance().reset();
    LogTestController::getInstance().setTrace<c2::C2Agent>();
    configuration_->set(Configuration::nifi_c2_agent_heartbeat_period, "100 ms");
  }

  C2AgentWatchdogConfigurationTestFixture(C2AgentWatchdogConfigurationTestFixture&&) = delete;
  C2AgentWatchdogConfigurationTestFixture(const C2AgentWatchdogConfigurationTestFixture&) = delete;
  C2AgentWatchdogConfigurationTestFixture& operator=(C2AgentWatchdogConfigurationTestFixture&&) = delete;
  C2AgentWatchdogConfigurationTestFixture& operator=(const C2AgentWatchdogConfigurationTestFixture&) = delete;

  ~C2AgentWatchdogConfigurationTestFixture() {
    if (agent_) {
      agent_->stop();
    }
    LogTestController::getInstance().reset();
  }

  void startAgent() {
    agent_ = std::make_unique<c2::C2Agent>(configuration_,
        std::weak_ptr<state::response::NodeReporter>{},
        std::make_shared<minifi::utils::file::FileSystem>(),
        recorder_.callback(),
        nullptr);
    agent_->initialize(nullptr, nullptr, nullptr);
    agent_->start();
  }

 protected:
  std::shared_ptr<Configure> configuration_ = std::make_shared<ConfigureImpl>();
  C2ShutdownRecorder recorder_;
  std::unique_ptr<c2::C2Agent> agent_;
};

TEST_CASE_METHOD(C2AgentWatchdogConfigurationTestFixture, "No shutdown is requested when the C2 connection timeout is not set", "[c2agentwatchdog]") {
  startAgent();
  REQUIRE(LogTestController::getInstance().contains("Performing heartbeat", 1s));
  std::this_thread::sleep_for(1s);
  REQUIRE(recorder_.count() == 0);
}

TEST_CASE_METHOD(C2AgentWatchdogConfigurationTestFixture, "C2 connection timeout of 0 disables the watchdog", "[c2agentwatchdog]") {
  configuration_->set(Configuration::nifi_c2_connection_timeout, "0 sec");
  startAgent();
  REQUIRE(LogTestController::getInstance().contains("Performing heartbeat", 1s));
  std::this_thread::sleep_for(1s);
  REQUIRE(recorder_.count() == 0);
  REQUIRE_FALSE(LogTestController::getInstance().contains("Failed to parse C2 configuration parameter", 0ms));
}

TEST_CASE_METHOD(C2AgentWatchdogConfigurationTestFixture, "Invalid C2 connection timeout disables the watchdog", "[c2agentwatchdog]") {
  configuration_->set(Configuration::nifi_c2_connection_timeout, "abc");
  startAgent();
  REQUIRE(LogTestController::getInstance().contains("Failed to parse C2 configuration parameter 'nifi.c2.connection.timeout': 'abc'", 1s));
  std::this_thread::sleep_for(1s);
  REQUIRE(recorder_.count() == 0);
}

TEST_CASE_METHOD(C2AgentWatchdogConfigurationTestFixture, "C2 connection timeout shorter than the heartbeat period is increased",
    "[c2agentwatchdog]") {
  configuration_->set(Configuration::nifi_c2_connection_timeout, "50 ms");
  startAgent();
  REQUIRE(LogTestController::getInstance().contains("C2 connection timeout is less than the heartbeat period", 1s));
  REQUIRE(LogTestController::getInstance().contains("Using 200ms as the C2 connection timeout", 1s));
}

TEST_CASE_METHOD(C2AgentWatchdogConfigurationTestFixture, "Failed heartbeats do not prevent the C2 connection timeout", "[c2agentwatchdog]") {
  configuration_->set(Configuration::nifi_c2_connection_timeout, "300 ms");
  startAgent();
  REQUIRE(utils::verifyEventHappenedInPollTime(3s, [&] { return recorder_.count() > 0; }, 10ms));
  REQUIRE(LogTestController::getInstance().contains("Performing heartbeat", 0ms));
  REQUIRE(recorder_.count() == 1);
  const auto [shutdown_type, exit_code] = recorder_.firstCall();
  CHECK(shutdown_type == ShutdownType::SHUTDOWN);
  REQUIRE(exit_code.has_value());
  CHECK(*exit_code == c2::C2_CONNECTION_TIMEOUT_EXIT_CODE);
}

TEST_CASE_METHOD(C2AgentWatchdogConfigurationTestFixture, "Stopping the C2 agent stops the watchdog", "[c2agentwatchdog]") {
  configuration_->set(Configuration::nifi_c2_connection_timeout, "300 ms");
  startAgent();
  agent_->stop();
  std::this_thread::sleep_for(600ms);
  REQUIRE(recorder_.count() == 0);
}

}  // namespace org::apache::nifi::minifi::test
