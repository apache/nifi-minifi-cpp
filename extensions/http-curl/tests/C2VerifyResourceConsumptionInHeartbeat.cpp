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

#undef NDEBUG
#include <functional>
#include <utility>

#include "TestBase.h"
#include "c2/C2Agent.h"
#include "protocols/RESTProtocol.h"
#include "protocols/RESTSender.h"
#include "protocols/RESTReceiver.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "utils/IntegrationTestUtils.h"
#include "properties/Configuration.h"

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
    assert(root.HasMember("deviceInfo"));
    auto& device_info = root["deviceInfo"];

    assert(device_info.HasMember("systemInfo"));
    auto& system_info = device_info["systemInfo"];

    assert(system_info.HasMember("vCores"));
    assert(system_info["vCores"].GetUint() > 0);

    assert(system_info.HasMember("physicalMem"));
    assert(system_info["physicalMem"].GetUint64() > 0);

    assert(system_info.HasMember("memoryUsage"));
    assert(system_info["memoryUsage"].GetUint64() > 0);

    assert(system_info.HasMember("cpuUtilization"));
    if (!firstCall) {
      assert(system_info["cpuUtilization"].GetDouble() >= 0.0);
      assert(system_info["cpuUtilization"].GetDouble() <= 1.0);
    }

    assert(system_info.HasMember("machineArch"));
    assert(system_info["machineArch"].GetStringLength() > 0);

#ifndef WIN32
    assert(system_info.HasMember("cpuLoadAverage"));
    assert(system_info["cpuLoadAverage"].GetDouble() >= 0.0);
#endif
  }

  static void verifyProcessResourceConsumption(const rapidjson::Document& root, bool firstCall) {
    assert(root.HasMember("agentInfo"));
    auto& agent_info = root["agentInfo"];

    assert(agent_info.HasMember("status"));
    auto& status = agent_info["status"];

    assert(status.HasMember("resourceConsumption"));
    auto& resource_consumption = status["resourceConsumption"];

    assert(resource_consumption.HasMember("memoryUsage"));
    assert(resource_consumption["memoryUsage"].GetUint64() > 0);

    assert(resource_consumption.HasMember("cpuUtilization"));
    auto& cpu_utilization = resource_consumption["cpuUtilization"];
    assert(cpu_utilization.IsDouble());
    if (!firstCall) {
      assert(cpu_utilization.GetDouble() >= 0.0);
      assert(cpu_utilization.GetDouble() <= 1.0);
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
    LogTestController::getInstance().setDebug<minifi::c2::RESTReceiver>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(std::chrono::milliseconds(7000), event_to_wait_for_));
  }

  void configureFullHeartbeat() override {
    configuration->set(minifi::Configuration::nifi_c2_full_heartbeat, "false");
  }

  void setEventToWaitFor(std::function<bool()> event_to_wait_for) {
    event_to_wait_for_ = std::move(event_to_wait_for);
  }

  std::function<bool()> event_to_wait_for_;
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv, "heartbeat");
  VerifyResourceConsumptionInHeartbeat harness;
  ResourceConsumptionInHeartbeatHandler responder(harness.getConfiguration());
  auto event_to_wait_for = [&responder] {
    return responder.getNumberOfHandledHeartBeats() >= 3;
  };

  harness.setUrl(args.url, &responder);
  harness.setEventToWaitFor(event_to_wait_for);
  harness.run(args.test_file);

  return 0;
}
