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
#include "protocols/RESTReceiver.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"

class LightWeightC2Handler : public HeartbeatHandler {
 public:
  explicit LightWeightC2Handler(bool isSecure)
      : HeartbeatHandler(isSecure) {
  }

  virtual ~LightWeightC2Handler() = default;

  virtual void handleHeartbeat(const rapidjson::Document& root, struct mg_connection *)  {
    if (calls_ == 0) {
      verifyJsonHasAgentManifest(root);
    } else {
      assert(root.HasMember("agentInfo"));
      assert(!root["agentInfo"].HasMember("agentManifest"));
    }
    calls_++;
  }
 private:
  std::atomic<size_t> calls_{0};
};

class VerifyC2Heartbeat : public VerifyC2Base {
 public:
  explicit VerifyC2Heartbeat(bool isSecure)
      : VerifyC2Base(isSecure) {
  }

  virtual void testSetup() {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTProtocol>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTReceiver>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() {
    assert(LogTestController::getInstance().contains("Received Ack from Server"));
    assert(LogTestController::getInstance().contains("C2Agent] [debug] Stopping component invoke"));
    assert(LogTestController::getInstance().contains("C2Agent] [debug] Stopping component FlowController"));
  }

  void configureFullHeartbeat() {
    configuration->set("nifi.c2.full.heartbeat", "true");
  }
};

class VerifyLightWeightC2Heartbeat : public VerifyC2Heartbeat {
public:
  explicit VerifyLightWeightC2Heartbeat(bool isSecure)
      : VerifyC2Heartbeat(isSecure) {
  }

  void configureFullHeartbeat() {
    configuration->set("nifi.c2.full.heartbeat", "false");
  }
};

int main(int argc, char **argv) {
  std::string key_dir, test_file_location, url;
  url = "http://localhost:0/api/heartbeat";
  if (argc > 1) {
    test_file_location = argv[1];
    if (argc > 2) {
      url = "https://localhost:0/api/heartbeat";
      key_dir = argv[2];
    }
  }

  bool isSecure = false;
  if (url.find("https") != std::string::npos) {
    isSecure = true;
  }
  {
    VerifyC2Heartbeat harness(isSecure);
    harness.setKeyDir(key_dir);
    HeartbeatHandler responder(isSecure);
    harness.setUrl(url, &responder);
    harness.run(test_file_location);
  }

  VerifyLightWeightC2Heartbeat harness(isSecure);
  harness.setKeyDir(key_dir);
  LightWeightC2Handler responder(isSecure);
  harness.setUrl(url, &responder);
  harness.run(test_file_location);

  return 0;
}
