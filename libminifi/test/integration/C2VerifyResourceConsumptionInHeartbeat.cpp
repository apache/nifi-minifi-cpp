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
#include <functional>
#include <utility>

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

class ResourceConsumptionInHeartbeatHandler : public HeartbeatHandler {
 public:
  explicit ResourceConsumptionInHeartbeatHandler(std::shared_ptr<minifi::Configure> configuration)
    : HeartbeatHandler(std::move(configuration)) {
  }

  void handleHeartbeat(const rapidjson::Document& root, struct mg_connection *) override {
    verifySystemResourceConsumption(root, (calls_ == 0));
    verifyProcessResourceConsumption(root, (calls_ == 0));
    ++calls_;
  }

  size_t getNumberOfHandledHeartBeats() {
    return calls_;
  }

 protected:
  static void verifySystemResourceConsumption(const rapidjson::Document& root, bool firstCall) {
    REQUIRE(root.HasMember("deviceInfo"));
    auto& device_info = root["deviceInfo"];

    REQUIRE(device_info.HasMember("systemInfo"));
    auto& system_info = device_info["systemInfo"];

    REQUIRE(system_info.HasMember("vCores"));
    REQUIRE(system_info["vCores"].GetUint() > 0);

    REQUIRE(system_info.HasMember("physicalMem"));
    REQUIRE(system_info["physicalMem"].GetUint64() > 0);

    REQUIRE(system_info.HasMember("memoryUsage"));
    REQUIRE(system_info["memoryUsage"].GetUint64() > 0);

    REQUIRE(system_info.HasMember("cpuUtilization"));
    if (!firstCall) {
      REQUIRE(system_info["cpuUtilization"].GetDouble() >= 0.0);
      REQUIRE(system_info["cpuUtilization"].GetDouble() <= 1.0);
    }

    REQUIRE(system_info.HasMember("machineArch"));
    REQUIRE(system_info["machineArch"].GetStringLength() > 0);

#ifndef WIN32
    REQUIRE(system_info.HasMember("cpuLoadAverage"));
    REQUIRE(system_info["cpuLoadAverage"].GetDouble() >= 0.0);
#endif
  }

  static void verifyProcessResourceConsumption(const rapidjson::Document& root, bool firstCall) {
    REQUIRE(root.HasMember("agentInfo"));
    auto& agent_info = root["agentInfo"];

    REQUIRE(agent_info.HasMember("status"));
    auto& status = agent_info["status"];

    REQUIRE(status.HasMember("resourceConsumption"));
    auto& resource_consumption = status["resourceConsumption"];

    REQUIRE(resource_consumption.HasMember("memoryUsage"));
    REQUIRE(resource_consumption["memoryUsage"].GetUint64() > 0);

    REQUIRE(resource_consumption.HasMember("cpuUtilization"));
    auto& cpu_utilization = resource_consumption["cpuUtilization"];
    REQUIRE(cpu_utilization.IsDouble());
    if (!firstCall) {
      REQUIRE(cpu_utilization.GetDouble() >= 0.0);
      REQUIRE(cpu_utilization.GetDouble() <= 1.0);
    }
  }

 private:
  std::atomic<size_t> calls_{0};
};

class VerifyResourceConsumptionInHeartbeat : public VerifyC2Base {
 public:
  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTProtocol>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    REQUIRE(utils::verifyEventHappenedInPollTime(std::chrono::milliseconds(7000), event_to_wait_for_));
  }

  void configureFullHeartbeat() override {
    configuration->set(minifi::Configuration::nifi_c2_full_heartbeat, "false");
  }

  void setEventToWaitFor(std::function<bool()> event_to_wait_for) {
    event_to_wait_for_ = std::move(event_to_wait_for);
  }

  std::function<bool()> event_to_wait_for_;
};

TEST_CASE("Verify resource consumption in C2 heartbeat", "[c2test]") {
  VerifyResourceConsumptionInHeartbeat harness;
  ResourceConsumptionInHeartbeatHandler responder(harness.getConfiguration());
  auto event_to_wait_for = [&responder] {
    return responder.getNumberOfHandledHeartBeats() >= 3;
  };

  harness.setUrl("http://localhost:0/heartbeat", &responder);
  harness.setEventToWaitFor(event_to_wait_for);
  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "C2VerifyHeartbeatAndStop.yml";
  harness.run(test_file_path);
}

}  // namespace org::apache::nifi::minifi::test
