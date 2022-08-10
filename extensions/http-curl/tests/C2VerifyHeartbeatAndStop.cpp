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
#include "Catch.h"
#include "c2/C2Agent.h"
#include "protocols/RESTProtocol.h"
#include "protocols/RESTSender.h"
#include "protocols/RESTReceiver.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "utils/IntegrationTestUtils.h"
#include "properties/Configuration.h"

class VerifyC2Heartbeat : public VerifyC2Base {
 public:
  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTProtocol>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTReceiver>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_),
        "Received Ack from Server",
        "C2Agent] [debug] Stopping component 2438e3c8-015a-1000-79ca-83af40ec1991",
        "C2Agent] [debug] Stopping component FlowController"));
  }

  void configureFullHeartbeat() override {
    configuration->set(minifi::Configuration::nifi_c2_full_heartbeat, "true");
  }
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv, "heartbeat");
  VerifyC2Heartbeat harness;
  harness.setKeyDir(args.key_dir);
  StoppingHeartbeatHandler responder(harness.getConfiguration());
  harness.setUrl(args.url, &responder);
  harness.run(args.test_file);

  return 0;
}
