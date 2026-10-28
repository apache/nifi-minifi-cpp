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
#include <string>

#include "c2/C2Agent.h"
#include "c2/C2ExitCodes.h"
#include "integration/HTTPIntegrationBase.h"
#include "integration/ServerAwareHandler.h"
#include "properties/Configuration.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class FailingHeartbeatHandler : public ServerAwareHandler {
 public:
  bool handlePost(CivetServer* /*server*/, struct mg_connection* conn) override {
    ++heartbeat_count_;
    if (fail_) {
      mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    } else {
      static const std::string response = R"({"operation": "heartbeat", "requested_operations": []})";
      mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n", response.length());
      mg_printf(conn, "%s", response.c_str());
    }
    return true;
  }

  std::atomic<bool> fail_{false};
  std::atomic<int> heartbeat_count_{0};
};

class VerifyC2ConnectionTimeout : public VerifyC2Base {
 public:
  VerifyC2ConnectionTimeout(const std::filesystem::path& test_file_path, FailingHeartbeatHandler& handler)
    : VerifyC2Base(test_file_path),
      handler_(handler) {
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();
    VerifyC2Base::testSetup();
  }

  void configureC2() override {
    VerifyC2Base::configureC2();
    configuration->set(minifi::Configuration::nifi_c2_agent_heartbeat_period, "100 ms");
    configuration->set(minifi::Configuration::nifi_c2_connection_timeout, "1 sec");
  }

  void runAssertions() override {
    // successful heartbeats keep the agent alive well past the connection timeout
    std::this_thread::sleep_for(3s);
    REQUIRE(handler_.heartbeat_count_ > 0);
    REQUIRE(shutdown_requested_count_ == 0);

    handler_.fail_ = true;
    REQUIRE(utils::verifyEventHappenedInPollTime(5s, [&] { return shutdown_requested_count_ > 0; }));
    CHECK(shutdown_requested_count_ == 1);
    CHECK(last_shutdown_exit_code_ == minifi::c2::C2_CONNECTION_TIMEOUT_EXIT_CODE);
    CHECK(utils::verifyLogLinePresenceInPollTime(1s, "C2 connection timeout exceeded, shutting down..."));
  }

 private:
  FailingHeartbeatHandler& handler_;
};

TEST_CASE("C2 agent requests shutdown when the C2 server stops responding successfully", "[c2test]") {
  FailingHeartbeatHandler handler;
  VerifyC2ConnectionTimeout harness(std::filesystem::path(TEST_RESOURCES) / "TestEmpty.yml", handler);
  harness.setUrl("http://localhost:0/heartbeat", &handler);
  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
