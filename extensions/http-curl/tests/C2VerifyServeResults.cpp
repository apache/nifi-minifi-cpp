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
#include "core/ProcessGroup.h"
#include "properties/Configure.h"
#include "TestServer.h"
#include "HTTPIntegrationBase.h"
#include "utils/IntegrationTestUtils.h"

class VerifyC2Server : public HTTPIntegrationBase {
public:
  explicit VerifyC2Server() {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<processors::InvokeHTTP>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessSession>();
    std::fstream file;
    ss << dir << "/" << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();
  }

  void cleanup() override {
    std::remove(ss.str().c_str());
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_),
        "Import offset 0",
        "Outputting success and response"));
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) override {
    std::shared_ptr<core::Processor> proc = pg->findProcessorByName("invoke");
    assert(proc != nullptr);

    std::shared_ptr<minifi::processors::InvokeHTTP> inv = std::dynamic_pointer_cast<minifi::processors::InvokeHTTP>(proc);

    assert(inv != nullptr);
    std::string url;
    inv->getProperty(minifi::processors::InvokeHTTP::URL.getName(), url);

    std::string port, scheme, path;
    parse_http_components(url, port, scheme, path);
    configuration->set("nifi.c2.enable", "true");
    configuration->set("nifi.c2.agent.class", "test");
    configuration->set("nifi.c2.agent.heartbeat.reporter.classes", "RESTReceiver");
    configuration->set("nifi.c2.agent.protocol.class", "RESTSender");
    configuration->set("nifi.c2.rest.listener.port", port);
    configuration->set("nifi.c2.agent.heartbeat.period", "10");
    configuration->set("nifi.c2.rest.listener.heartbeat.rooturi", path);
  }

 protected:
  std::string dir;
  std::stringstream ss;
  TestController testController;
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv);

  VerifyC2Server harness;
  harness.setKeyDir(args.key_dir);
  harness.run(args.test_file);

  return 0;
}
