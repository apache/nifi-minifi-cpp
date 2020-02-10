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

#include <sys/stat.h>
#undef NDEBUG
#include <cassert>
#include <utility>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <iostream>
#include <sstream>
#include "HTTPClient.h"
#include "InvokeHTTP.h"
#include "TestBase.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "c2/C2Agent.h"
#include "CivetServer.h"
#include <cstring>
#include "protocols/RESTSender.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"

class VerifyC2Describe : public VerifyC2Base {
 public:
  explicit VerifyC2Describe(bool isSecure)
      : VerifyC2Base(isSecure) {
  }

  virtual ~VerifyC2Describe() = default;

  void testSetup() {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    VerifyC2Base::testSetup();
  }

  void configureC2RootClasses() {
    configuration->set("nifi.c2.root.classes", "DeviceInfoNode,AgentInformationWithoutManifest,FlowInformation");
  }
};

class VerifyC2DescribeJstack : public VerifyC2Describe {
 public:
  explicit VerifyC2DescribeJstack(bool isSecure)
      : VerifyC2Describe(isSecure) {
  }

  virtual ~VerifyC2DescribeJstack() = default;

  virtual void runAssertions() {
    assert(LogTestController::getInstance().contains("SchedulingAgent") == true);
  }

};

class DescribeManifestHandler: public HeartbeatHandler {
public:

  explicit DescribeManifestHandler(bool isSecure)
      : HeartbeatHandler(isSecure) {
  }

  virtual ~DescribeManifestHandler() =  default;

  virtual void handleHeartbeat(const rapidjson::Document& root, struct mg_connection * conn) {
    sendHeartbeatResponse("DESCRIBE", "manifest", "889345", conn);
  }

  virtual void handleAcknowledge(const rapidjson::Document& root) {
    verifyJsonHasAgentManifest(root);
  }
};

class DescribeJstackHandler : public HeartbeatHandler {
 public:
  explicit DescribeJstackHandler(bool isSecure)
     : HeartbeatHandler(isSecure) {
  }

  virtual ~DescribeJstackHandler() = default;

  virtual void handleHeartbeat(const rapidjson::Document& root, struct mg_connection * conn) {
    sendHeartbeatResponse("DESCRIBE", "jstack", "889398", conn);
  }

  virtual void handleAcknowledge(const rapidjson::Document& root) {
    assert(root.HasMember("SchedulingAgent #0") == true);
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
    VerifyC2Describe harness(isSecure);

    harness.setKeyDir(key_dir);

    DescribeManifestHandler responder(isSecure);

    harness.setUrl(url, &responder);

    harness.run(test_file_location);
  }

  VerifyC2Describe harness(isSecure);

  harness.setKeyDir(key_dir);

  DescribeJstackHandler responder(isSecure);

  harness.setUrl(url, &responder);

  harness.run(test_file_location);

}
