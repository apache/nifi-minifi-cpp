// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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

class VerifyC2Heartbeat : public VerifyC2Base {
 public:
  using VerifyC2Base::VerifyC2Base;
  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTProtocol>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    REQUIRE(utils::verifyLogLinePresenceInPollTime(20s,
        "Received Ack from Server",
        "C2Agent] [debug] Stopping processor 2438e3c8-015a-1000-79ca-83af40ec1991",
        "C2Agent] [debug] Stopping all processors",
        "C2Agent] [debug] Starting processor 2438e3c8-015a-1000-79ca-83af40ec1991",
        "C2Agent] [debug] Starting all processors",
        "C2Agent] [debug] Stopping processor 2438e3c8-015a-1000-79ca-83af40ec1992",
        "C2Agent] [debug] Starting processor 2438e3c8-015a-1000-79ca-83af40ec1992",
        "[error] Could not get execute requested callback for component \"9998e3c8-015a-1000-79ca-83af40ec1999\", because component was not found",
        "[warning] Processor start/stop request missing 'processorId' argument"));
    CHECK(utils::countLogOccurrencesUntil("C2Agent] [debug] Stopping all processors", 2, 10s, 100ms));
    CHECK(utils::countLogOccurrencesUntil("C2Agent] [debug] Starting all processors", 2, 10s, 100ms));
  }

  void configureFullHeartbeat() override {
    configuration->set(minifi::Configuration::nifi_c2_full_heartbeat, "true");
    configuration->set(minifi::Configuration::nifi_c2_agent_heartbeat_period, "100");
  }
};

TEST_CASE("Verify C2 heartbeat and stop operation", "[c2test]") {
  VerifyC2Heartbeat harness(std::filesystem::path(TEST_RESOURCES) / "C2VerifyHeartbeatAndStopSecure.yml");
  StoppingHeartbeatHandler responder(harness.getConfiguration());
  SECTION("Secure") {
    harness.setKeyDir(TEST_RESOURCES);
    harness.setUrl("https://localhost:0/heartbeat", &responder);
  }
  SECTION("Insecure") {
    harness.setUrl("http://localhost:0/heartbeat", &responder);
  }
  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
