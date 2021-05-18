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
#include "TestBase.h"
#include "c2/C2Agent.h"
#include "protocols/RESTProtocol.h"
#include "protocols/RESTSender.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "utils/IntegrationTestUtils.h"
#include "c2/HeartbeatLogger.h"

class VerifyLogC2Heartbeat : public VerifyC2Base {
 public:
  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTProtocol>();
    // the heartbeat is logged at TRACE level
    LogTestController::getInstance().setTrace<minifi::c2::HeartbeatLogger>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(
        std::chrono::milliseconds(wait_time_),
        "\"operation\": \"heartbeat\""));
  }

  void configureC2() override {
    VerifyC2Base::configureC2();
    configuration->set("nifi.c2.agent.heartbeat.reporter.classes", "HeartbeatLogger");
  }
};

int main() {
  VerifyLogC2Heartbeat harness;
  HeartbeatHandler responder;
  harness.setUrl("https://localhost:0/heartbeat", &responder);
  harness.run();
}
