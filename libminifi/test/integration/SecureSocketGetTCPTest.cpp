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
#include <signal.h>
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
#include "processors/GetTCP.h"
#include "../TestBase.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "controllers/SSLContextService.h"
#include "c2/C2Agent.h"
#include "IntegrationBase.h"
#include "processors/LogAttribute.h"
#include "io/tls/TLSSocket.h"
#include "io/tls/TLSServerSocket.h"

class SecureSocketTest : public IntegrationBase {
 public:
  explicit SecureSocketTest(bool isSecure)
      : isSecure(isSecure) {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  void testSetup() {
    LogTestController::getInstance().setDebug<minifi::io::Socket>();
    LogTestController::getInstance().setDebug<minifi::io::TLSContext>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
    LogTestController::getInstance().setTrace<minifi::io::TLSSocket>();
    LogTestController::getInstance().setTrace<processors::GetTCP>();
    std::fstream file;
    ss << dir << "/" << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();
  }

  void cleanup() {
    LogTestController::getInstance().reset();
    unlink(ss.str().c_str());
  }

  void runAssertions() {
    assert(LogTestController::getInstance().contains("send succeed 20") == true);
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) {
    std::shared_ptr<core::Processor> proc = pg->findProcessor("invoke");
    assert(proc != nullptr);

    std::shared_ptr<minifi::processors::GetTCP> inv = std::dynamic_pointer_cast<minifi::processors::GetTCP>(proc);

    assert(inv != nullptr);
    std::string url = "";
    configuration->set("nifi.remote.input.secure", "true");
    std::string path = key_dir + "cn.crt.pem";
    configuration->set("nifi.security.client.certificate", path);
    path = key_dir + "cn.ckey.pem";
    configuration->set("nifi.security.client.private.key", path);
    path = key_dir + "cn.pass";
    configuration->set("nifi.security.client.pass.phrase", path);
    path = key_dir + "nifi-cert.pem";
    configuration->set("nifi.security.client.ca.certificate", path);
    configuration->set("nifi.c2.enable", "false");
    std::shared_ptr<org::apache::nifi::minifi::io::TLSContext> socket_context = std::make_shared<org::apache::nifi::minifi::io::TLSContext>(configuration);
    server_socket = std::make_shared<org::apache::nifi::minifi::io::TLSServerSocket>(socket_context, "localhost", 8776, 3);
    server_socket->initialize();

    isRunning_ = true;
    check = [this]() -> bool {
      return isRunning_;
    };
    handler = [this](std::vector<uint8_t> *b, int *size) {
      b->reserve(20);
      memset(b->data(), 0x00, 20);
      memcpy(b->data(), "hello world", 11);
      *size = 20;
      return *size;
    };
    server_socket->registerCallback(check, handler);
  }

  void run(std::string test_file_location) {
    testSetup();

    std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
    std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

    configuration->set(minifi::Configure::nifi_flow_configuration_file, test_file_location);

    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(configuration);
    std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);
    std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<core::YamlConfiguration>(
        new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location));

    core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location);

    std::unique_ptr<core::ProcessGroup> ptr = yaml_config.getRoot(test_file_location);
    std::shared_ptr<core::ProcessGroup> pg = std::shared_ptr<core::ProcessGroup>(ptr.get());

    queryRootProcessGroup(pg);

    ptr.release();

    std::shared_ptr<TestRepository> repo = std::static_pointer_cast<TestRepository>(test_repo);

    std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo, DEFAULT_ROOT_GROUP_NAME,
                                                                                                  true);
    controller->load();
    controller->start();
    waitToVerifyProcessor();
    controller->waitUnload(60000);
    isRunning_ = false;
    server_socket->closeStream();
    server_socket = nullptr;
    runAssertions();

    cleanup();
  }

  virtual void waitToVerifyProcessor() {
      std::this_thread::sleep_for(std::chrono::seconds(3));
    }

 protected:
  std::function<bool()> check;
  std::function<int(std::vector<uint8_t>*b, int *size)> handler;
  std::atomic<bool> isRunning_;
  bool isSecure;
  char *dir;
  std::stringstream ss;
  TestController testController;
  std::shared_ptr<org::apache::nifi::minifi::io::TLSServerSocket> server_socket;
};

static void sigpipe_handle(int x) {}

int main(int argc, char **argv) {
  std::string key_dir, test_file_location, url;
  url = "http://localhost:8888/api/heartbeat";
  if (argc > 1) {
    test_file_location = argv[1];
    key_dir = argv[2];
  }

  signal(SIGPIPE, sigpipe_handle);

  SecureSocketTest harness(true);

  harness.setKeyDir(key_dir);

  harness.run(test_file_location);

  return 0;
}
