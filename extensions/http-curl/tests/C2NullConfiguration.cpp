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
#include "CivetServer.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "controllers/SSLContextService.h"
#include "TestServer.h"
#include "protocols/RESTReceiver.h"
#include "protocols/RESTSender.h"
#include "c2/C2Agent.h"
#include "processors/LogAttribute.h"
#include "HTTPIntegrationBase.h"

class VerifyC2Server : public CoapIntegrationBase {
 public:
  explicit VerifyC2Server(bool isSecure)
      : isSecure(isSecure) {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  void testSetup() {
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setDebug<processors::InvokeHTTP>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTReceiver>();
    LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessSession>();
    std::fstream file;
    ss << dir << "/" << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();
  }

  void cleanup() {
    unlink(ss.str().c_str());
  }

  void runAssertions() {
    assert(LogTestController::getInstance().contains("C2Agent] [debug] Could not instantiate null") == true);
    assert(LogTestController::getInstance().contains("Class is RESTSender") == true);
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) {
    std::shared_ptr<core::Processor> proc = pg->findProcessor("invoke");
    assert(proc != nullptr);

    std::shared_ptr<minifi::processors::InvokeHTTP> inv = std::dynamic_pointer_cast<minifi::processors::InvokeHTTP>(proc);

    assert(inv != nullptr);
    std::string url = "";
    inv->getProperty(minifi::processors::InvokeHTTP::URL.getName(), url);

    std::string port, scheme, path;
    parse_http_components(url, port, scheme, path);
    configuration->set("c2.enable", "true");
    configuration->set("c2.agent.class", "test");
    configuration->set("c2.agent.protocol.class", "RESTSender");
    configuration->set("c2.rest.url", "");
    configuration->set("c2.rest.url.ack", "");
    configuration->set("c2.agent.heartbeat.reporter.classes", "null");
    configuration->set("c2.rest.listener.port", "null");
    configuration->set("c2.agent.heartbeat.period", "null");
    configuration->set("c2.rest.listener.heartbeat.rooturi", "null");
  }

 protected:
  bool isSecure;
  char *dir;
  std::stringstream ss;
  TestController testController;
};

int main(int argc, char **argv) {
  std::string key_dir, test_file_location, url;
  if (argc > 1) {
    test_file_location = argv[1];
    key_dir = argv[2];
  }

  bool isSecure = false;
  if (url.find("https") != std::string::npos) {
    isSecure = true;
  }

  VerifyC2Server harness(isSecure);

  harness.setKeyDir(key_dir);

  harness.run(test_file_location);

  return 0;
}

