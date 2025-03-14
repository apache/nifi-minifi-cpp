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
#include <mutex>

#include "unit/TestBase.h"
#include "c2/C2Agent.h"
#include "protocols/RESTProtocol.h"
#include "protocols/RESTSender.h"
#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "unit/TestUtils.h"
#include "properties/Configuration.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class LightWeightC2Handler : public StoppingHeartbeatHandler {
 public:
  explicit LightWeightC2Handler(std::shared_ptr<minifi::Configure> configuration)
    : StoppingHeartbeatHandler(std::move(configuration)) {
  }

  void handleHeartbeat(const rapidjson::Document& root, struct mg_connection *) override {
    std::lock_guard<std::mutex> lock(call_mutex_);
    if (calls_ == 0) {
      verifyJsonHasAgentManifest(root);
    } else {
      REQUIRE(root.HasMember("agentInfo"));
      REQUIRE(!root["agentInfo"].HasMember("agentManifest"));
    }
    calls_++;
  }

 private:
  std::mutex call_mutex_;
  std::size_t calls_{0};
};

class VerifyLightWeightC2Heartbeat : public VerifyC2Base {
 public:
  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTProtocol>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_),
        "Received Ack from Server",
        "C2Agent] [debug] Stopping component 2438e3c8-015a-1000-79ca-83af40ec1991",
        "C2Agent] [debug] Stopping component FlowController"));
  }

  void configureFullHeartbeat() override {
    configuration->set(minifi::Configuration::nifi_c2_full_heartbeat, "false");
  }
};

TEST_CASE("Verify C2 lightweight heartbeat and stop operation", "[c2test]") {
  VerifyLightWeightC2Heartbeat harness;
  LightWeightC2Handler responder(harness.getConfiguration());
  SECTION("Secure") {
    harness.setKeyDir(TEST_RESOURCES);
    harness.setUrl("https://localhost:0/heartbeat", &responder);
  }
  SECTION("Insecure") {
    harness.setUrl("http://localhost:0/heartbeat", &responder);
  }
  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "C2VerifyHeartbeatAndStopSecure.yml";
  harness.run(test_file_path);
}

}  // namespace org::apache::nifi::minifi::test
