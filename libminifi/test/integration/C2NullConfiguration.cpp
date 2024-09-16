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
#include <memory>
#include <string>
#include <iostream>
#include "processors/InvokeHTTP.h"
#include "unit/TestBase.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "integration/TestServer.h"
#include "protocols/RESTReceiver.h"
#include "c2/C2Agent.h"
#include "processors/LogAttribute.h"
#include "integration/HTTPIntegrationBase.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class VerifyC2Server : public HTTPIntegrationBase {
 public:
  explicit VerifyC2Server(bool isSecure)
      : isSecure(isSecure) {
    dir = testController.createTempDirectory();
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::http::HTTPClient>();
    LogTestController::getInstance().setDebug<processors::InvokeHTTP>();
    LogTestController::getInstance().setDebug<c2::RESTReceiver>();
    LogTestController::getInstance().setDebug<c2::C2Agent>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
    std::fstream file;
    auto path = dir / "tstFile.ext";
    file.open(path, std::ios::out);
    file << "tempFile";
    file.close();
  }

  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_), "C2Agent] [error] Could not instantiate null"));
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) override {
    auto* const proc = pg->findProcessorByName("invoke");
    REQUIRE(proc != nullptr);

    const auto* const inv = dynamic_cast<minifi::processors::InvokeHTTP*>(proc);

    REQUIRE(inv != nullptr);
    std::string url = inv->getProperty(processors::InvokeHTTP::URL.name).value_or("");

    std::string port;
    std::string scheme;
    std::string path;
    minifi::utils::parse_http_components(url, port, scheme, path);
    configuration->set(Configuration::nifi_c2_enable, "true");
    configuration->set(Configuration::nifi_c2_agent_class, "test");
    configuration->set(Configuration::nifi_c2_rest_url, "");
    configuration->set(Configuration::nifi_c2_rest_url_ack, "");
    configuration->set(Configuration::nifi_c2_agent_heartbeat_reporter_classes, "null");
    configuration->set(Configuration::nifi_c2_rest_listener_port, "null");
    configuration->set(Configuration::nifi_c2_agent_heartbeat_period, "null");
  }

 protected:
  bool isSecure;
  std::filesystem::path dir;
  TestController testController;
};

TEST_CASE("C2NullConfiguration", "[c2test]") {
  VerifyC2Server harness(true);
  harness.setKeyDir(TEST_RESOURCES);
  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestNull.yml";
  harness.run(test_file_path);
}

}  // namespace org::apache::nifi::minifi::test
