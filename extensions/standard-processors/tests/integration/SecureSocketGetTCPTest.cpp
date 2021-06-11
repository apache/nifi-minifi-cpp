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
#include <chrono>
#include <thread>
#undef NDEBUG
#include <cassert>
#include <utility>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include <iostream>
#include <sstream>
#include "HTTPClient.h"
#include "processors/GetTCP.h"
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
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "controllers/SSLContextService.h"
#include "c2/C2Agent.h"
#include "integration/IntegrationBase.h"
#include "processors/LogAttribute.h"
#include "io/tls/TLSSocket.h"
#include "io/tls/TLSServerSocket.h"
#include "utils/IntegrationTestUtils.h"

class SecureSocketTest : public IntegrationBase {
 public:
  explicit SecureSocketTest(bool isSecure)
      : isSecure{ isSecure }, isRunning_{ false } {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::io::Socket>();
    LogTestController::getInstance().setDebug<minifi::io::TLSContext>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
    LogTestController::getInstance().setTrace<minifi::io::TLSSocket>();
    LogTestController::getInstance().setTrace<processors::GetTCP>();
    std::fstream file;
    ss << dir << utils::file::FileUtils::get_separator() << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();
  }

  void cleanup() override {
    LogTestController::getInstance().reset();
    IntegrationBase::cleanup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_), "SSL socket connect success"));
    isRunning_ = false;
    server_socket_.reset();
    assert(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_), "send succeed 20"));
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) override {
    std::shared_ptr<core::Processor> proc = pg->findProcessorByName("invoke");
    assert(proc != nullptr);

    std::shared_ptr<minifi::processors::GetTCP> inv = std::dynamic_pointer_cast<minifi::processors::GetTCP>(proc);

    assert(inv != nullptr);
    std::string url;
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
    std::string endpoint;
    inv->getProperty(minifi::processors::GetTCP::EndpointList.getName(), endpoint);
    auto endpoints = utils::StringUtils::split(endpoint, ",");
    assert(1 == endpoints.size());
    auto hostAndPort = utils::StringUtils::split(endpoint, ":");
    std::shared_ptr<org::apache::nifi::minifi::io::TLSContext> socket_context = std::make_shared<org::apache::nifi::minifi::io::TLSContext>(configuration);
    std::string host = hostAndPort.at(0);
    if (host == "localhost") {
      host = org::apache::nifi::minifi::io::Socket::getMyHostName();
    }
    server_socket_ = std::make_shared<org::apache::nifi::minifi::io::TLSServerSocket>(socket_context, host, std::stoi(hostAndPort.at(1)), 3);
    assert(0 == server_socket_->initialize());

    isRunning_ = true;
    auto handler = [](std::vector<uint8_t> *b) {
      *b = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', 0, 0, 0, 0, 0, 0, 0, 0, 0};
      assert(b->size() == 20);
      return b->size();
    };
    server_socket_->registerCallback([this] { return isRunning_.load(); }, std::move(handler), std::chrono::milliseconds(50));
  }

 protected:
  bool isSecure;
  std::atomic<bool> isRunning_;
  std::string dir;
  std::stringstream ss;
  TestController testController;
  std::shared_ptr<org::apache::nifi::minifi::io::TLSServerSocket> server_socket_;
};

static void sigpipe_handle(int /*x*/) {
}

int main(int argc, char **argv) {
  std::string key_dir, test_file_location;
  if (argc > 1) {
    test_file_location = argv[1];
    key_dir = argv[2];
  }

#ifndef WIN32
  signal(SIGPIPE, sigpipe_handle);
#endif
  SecureSocketTest harness(true);

  harness.setKeyDir(key_dir);

  harness.run(test_file_location);

  return 0;
}
