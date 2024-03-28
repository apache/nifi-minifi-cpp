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
#include "http/BaseHTTPClient.h"
#include "processors/InvokeHTTP.h"
#include "unit/TestBase.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "CivetServer.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "controllers/SSLContextService.h"
#include "c2/C2Agent.h"
#include "protocols/RESTReceiver.h"
#include "CoapIntegrationBase.h"
#include "processors/LogAttribute.h"
#include "CoapC2Protocol.h"
#include "CoapServer.h"
#include "concurrentqueue.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"

class VerifyCoAPServer : public CoapIntegrationBase {
 public:
  explicit VerifyCoAPServer(bool isSecure)
      : isSecure(isSecure) {
    dir = testController.createTempDirectory();
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::http::HTTPClient>();
    LogTestController::getInstance().setOff<minifi::processors::InvokeHTTP>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTReceiver>();
    LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();
    LogTestController::getInstance().setTrace<minifi::coap::c2::CoapProtocol>();
    LogTestController::getInstance().setOff<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setOff<minifi::core::ProcessSession>();
    std::fstream file;
    ss << dir << "/" << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();
  }

  void cleanup() override {
    unlink(ss.str().c_str());
    CoapIntegrationBase::cleanup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
    REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3),
        "Received ack. version 3. number of operations 1",
        "Received ack. version 3. number of operations 0",
        "Received read error event from protocol",
        "Received op 1, with id id and operand operand"));
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) override {
    auto proc = pg->findProcessorByName("invoke");
    REQUIRE(proc != nullptr);

    const auto* const inv = dynamic_cast<minifi::processors::InvokeHTTP*>(proc);

    REQUIRE(inv != nullptr);
    std::string url;
    inv->getProperty(minifi::processors::InvokeHTTP::URL, url);

    std::string port;
    std::string scheme;
    std::string path;

    minifi::utils::parse_http_components(url, port, scheme, path);
    uint16_t newport = std::stoi(port) + 2;
    auto new_port_str = std::to_string(newport);

    server = std::make_unique<minifi::coap::CoapServer>("127.0.0.1", newport);


    server->add_endpoint(minifi::coap::Method::Post, [](minifi::coap::CoapQuery)->minifi::coap::CoapResponse {
      minifi::coap::CoapResponse response(205, nullptr, 0);
      return response;
    });

    {
      // valid response version 3, 0 ops
      auto data = std::unique_ptr<uint8_t[]>(new uint8_t[5] { 0x00, 0x03, 0x00, 0x01, 0x00 });  // NOLINT(cppcoreguidelines-avoid-c-arrays)
      minifi::coap::CoapResponse response(205, std::move(data), 5);
      responses.enqueue(std::move(response));
    }

    {
      // valid response
      auto data = std::unique_ptr<uint8_t[]>(new uint8_t[5] { 0x00, 0x03, 0x00, 0x00, 0x00 });  // NOLINT(cppcoreguidelines-avoid-c-arrays)
      minifi::coap::CoapResponse response(205, std::move(data), 5);
      responses.enqueue(std::move(response));
    }

    {
      // should result in valid operation
      minifi::io::BufferStream stream;
      uint16_t version = 0;
      uint16_t size = 1;
      uint8_t operation = 1;
      stream.write(version);
      stream.write(size);
      stream.write(&operation, 1);
      stream.write("id");
      stream.write("operand");

      auto data = std::make_unique<uint8_t[]>(stream.size());  // NOLINT(cppcoreguidelines-avoid-c-arrays)
      memcpy(data.get(), stream.getBuffer().data(), stream.getBuffer().size());
      minifi::coap::CoapResponse response(205, std::move(data), stream.size());
      responses.enqueue(std::move(response));
    }

    server->add_endpoint("heartbeat", minifi::coap::Method::Post, [&](minifi::coap::CoapQuery)-> minifi::coap::CoapResponse {
      if (responses.size_approx() > 0) {
        minifi::coap::CoapResponse resp(500, nullptr, 0);;
        responses.try_dequeue(resp);
        return resp;
      }
      minifi::coap::CoapResponse response(500, nullptr, 0);
      return response;
    });
    server->start();
    configuration->set(minifi::Configuration::nifi_c2_enable, "true");
    configuration->set(minifi::Configuration::nifi_c2_agent_class, "test");
    configuration->set(minifi::Configuration::nifi_c2_root_classes, "DeviceInfoNode,AgentInformation,FlowInformation,RepositoryMetrics");
    configuration->set(minifi::Configuration::nifi_c2_agent_protocol_class, "CoapProtocol");
    configuration->set(minifi::Configuration::nifi_c2_agent_coap_host, "127.0.0.1");
    configuration->set(minifi::Configuration::nifi_c2_agent_coap_port, new_port_str);
    configuration->set(minifi::Configuration::nifi_c2_agent_heartbeat_period, "10");
  }

 protected:
  moodycamel::ConcurrentQueue<minifi::coap::CoapResponse> responses;
  std::unique_ptr<minifi::coap::CoapServer> server;
  bool isSecure;
  std::string dir;
  std::stringstream ss;
  TestController testController;
};

TEST_CASE("CoapC2VerifyHeartbeat", "[c2test]") {
  VerifyCoAPServer harness(false);
  harness.setKeyDir(TEST_RESOURCES);
  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "CoapC2VerifyServe.yml";
  harness.run(test_file_path);
}
