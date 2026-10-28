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
#include <atomic>
#include <memory>
#include <optional>
#include <string>

#include "c2/C2Agent.h"
#include "c2/C2ExitCodes.h"
#include "properties/Configure.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "utils/file/FileSystem.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class C2AgentConnectionTimeoutTestFixture {
 public:
  C2AgentConnectionTimeoutTestFixture() {
    LogTestController::getInstance().reset();
    LogTestController::getInstance().setTrace<c2::C2Agent>();
    configuration_->set(Configuration::nifi_c2_agent_heartbeat_period, "100 ms");
  }

  C2AgentConnectionTimeoutTestFixture(C2AgentConnectionTimeoutTestFixture&&) = delete;
  C2AgentConnectionTimeoutTestFixture(const C2AgentConnectionTimeoutTestFixture&) = delete;
  C2AgentConnectionTimeoutTestFixture& operator=(C2AgentConnectionTimeoutTestFixture&&) = delete;
  C2AgentConnectionTimeoutTestFixture& operator=(const C2AgentConnectionTimeoutTestFixture&) = delete;

  ~C2AgentConnectionTimeoutTestFixture() {
    if (agent_) {
      agent_->stop();
    }
    LogTestController::getInstance().reset();
  }

  void startAgent() {
    agent_ = std::make_unique<c2::C2Agent>(configuration_,
        std::weak_ptr<state::response::NodeReporter>{},
        std::make_shared<minifi::utils::file::FileSystem>(),
        [this](ShutdownType shutdown_type, std::optional<int32_t> exit_code) {
          last_shutdown_type_ = shutdown_type;
          last_exit_code_ = exit_code.value_or(0);
          ++shutdown_request_count_;
        },
        nullptr);
    agent_->initialize(nullptr, nullptr, nullptr);
    agent_->start();
  }

 protected:
  std::shared_ptr<Configure> configuration_ = std::make_shared<ConfigureImpl>();
  std::atomic<int> shutdown_request_count_{0};
  std::atomic<ShutdownType> last_shutdown_type_{ShutdownType::RESTART};
  std::atomic<int32_t> last_exit_code_{0};
  std::unique_ptr<c2::C2Agent> agent_;
};

TEST_CASE_METHOD(C2AgentConnectionTimeoutTestFixture, "No shutdown is requested when the C2 connection timeout is not set", "[c2connectiontimeout]") {
  startAgent();
  REQUIRE(LogTestController::getInstance().contains("Performing heartbeat", 1s));
  std::this_thread::sleep_for(1s);
  REQUIRE(shutdown_request_count_ == 0);
}

TEST_CASE_METHOD(C2AgentConnectionTimeoutTestFixture, "C2 connection timeout of 0 disables the C2 connection timeout", "[c2connectiontimeout]") {
  configuration_->set(Configuration::nifi_c2_connection_timeout, "0 sec");
  startAgent();
  REQUIRE(LogTestController::getInstance().contains("Performing heartbeat", 1s));
  std::this_thread::sleep_for(1s);
  REQUIRE(shutdown_request_count_ == 0);
  REQUIRE_FALSE(LogTestController::getInstance().contains("Failed to parse C2 configuration parameter", 0ms));
}

TEST_CASE_METHOD(C2AgentConnectionTimeoutTestFixture, "Invalid C2 connection timeout disables the C2 connection timeout", "[c2connectiontimeout]") {
  configuration_->set(Configuration::nifi_c2_connection_timeout, "abc");
  startAgent();
  REQUIRE(LogTestController::getInstance().contains("Failed to parse C2 configuration parameter 'nifi.c2.connection.timeout': 'abc'", 1s));
  std::this_thread::sleep_for(1s);
  REQUIRE(shutdown_request_count_ == 0);
}

TEST_CASE_METHOD(C2AgentConnectionTimeoutTestFixture, "C2 connection timeout shorter than the heartbeat period is increased",
    "[c2connectiontimeout]") {
  configuration_->set(Configuration::nifi_c2_connection_timeout, "50 ms");
  startAgent();
  REQUIRE(LogTestController::getInstance().contains("C2 connection timeout is less than the heartbeat period", 1s));
  REQUIRE(LogTestController::getInstance().contains("Using 200ms as the C2 connection timeout", 1s));
}

TEST_CASE_METHOD(C2AgentConnectionTimeoutTestFixture, "Failed heartbeats do not prevent the C2 connection timeout", "[c2connectiontimeout]") {
  configuration_->set(Configuration::nifi_c2_connection_timeout, "300 ms");
  startAgent();
  REQUIRE(utils::verifyEventHappenedInPollTime(3s, [&] { return shutdown_request_count_ > 0; }, 10ms));
  REQUIRE(LogTestController::getInstance().contains("Performing heartbeat", 0ms));
  std::this_thread::sleep_for(500ms);
  REQUIRE(shutdown_request_count_ == 1);
  CHECK(last_shutdown_type_ == ShutdownType::SHUTDOWN);
  CHECK(last_exit_code_ == c2::C2_CONNECTION_TIMEOUT_EXIT_CODE);
}

TEST_CASE_METHOD(C2AgentConnectionTimeoutTestFixture, "Stopping the C2 agent stops the C2 connection timeout check", "[c2connectiontimeout]") {
  configuration_->set(Configuration::nifi_c2_connection_timeout, "300 ms");
  startAgent();
  agent_->stop();
  std::this_thread::sleep_for(600ms);
  REQUIRE(shutdown_request_count_ == 0);
}

}  // namespace org::apache::nifi::minifi::test
