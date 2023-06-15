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
#include <cstdio>
#include <memory>
#include <string>
#include <sstream>
#include "processors/InvokeHTTP.h"
#include "TestBase.h"
#include "Catch.h"
#include "core/ProcessGroup.h"
#include "properties/Configure.h"
#include "TestServer.h"
#include "HTTPIntegrationBase.h"
#include "utils/IntegrationTestUtils.h"
#include "properties/Configuration.h"

class VerifyC2Server : public HTTPIntegrationBase {
 public:
  VerifyC2Server() {
    dir_ = testController.createTempDirectory();
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::processors::InvokeHTTP>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessSession>();
    std::fstream file;
    test_file_ = dir_ / "tstFile.ext";
    file.open(test_file_, std::ios::out);
    file << "tempFile";
    file.close();
  }

  void cleanup() override {
    std::filesystem::remove(test_file_);
    IntegrationBase::cleanup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_),
        "Import offset 0",
        "Outputting success and response"));
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) override {
    auto proc = pg->findProcessorByName("invoke");
    assert(proc != nullptr);

    auto inv = dynamic_cast<minifi::processors::InvokeHTTP*>(proc);

    assert(inv != nullptr);
    std::string url;
    inv->getProperty(minifi::processors::InvokeHTTP::URL, url);

    std::string port;
    std::string scheme;
    std::string path;
    minifi::utils::parse_http_components(url, port, scheme, path);
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_enable, "true");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_class, "test");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_heartbeat_reporter_classes, "RESTReceiver");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_protocol_class, "RESTSender");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_rest_listener_port, port);
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_heartbeat_period, "10");
  }

 protected:
  std::filesystem::path dir_;
  std::filesystem::path test_file_;
  TestController testController;
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv);

  VerifyC2Server harness;
  harness.setKeyDir(args.key_dir);
  harness.run(args.test_file);

  return 0;
}
