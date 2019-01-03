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
#include "processors/InvokeHTTP.h"
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
#include "c2/C2Agent.h"
#include "protocols/RESTReceiver.h"
#include "CoapIntegrationBase.h"
#include "processors/LogAttribute.h"
#include "CoapC2Protocol.h"
#include "CoapServer.h"
#include "io/BaseStream.h"
#include "concurrentqueue.h"

class Responder : public CivetHandler {
 public:
  explicit Responder(bool isSecure)
      : isSecure(isSecure) {
  }
  bool handlePost(CivetServer *server, struct mg_connection *conn) {
    std::string resp = "{\"operation\" : \"heartbeat\", \"requested_operations\" : [{ \"operationid\" : 41, \"operation\" : \"stop\", \"name\" : \"invoke\"  }, "
        "{ \"operationid\" : 42, \"operation\" : \"stop\", \"name\" : \"FlowController\"  } ]}";
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
              "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              resp.length());
    mg_printf(conn, "%s", resp.c_str());
    return true;
  }

 protected:
  bool isSecure;
};

class VerifyCoAPServer : public CoapIntegrationBase {
 public:
  explicit VerifyCoAPServer(bool isSecure)
      : isSecure(isSecure) {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  void testSetup() {
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setOff<processors::InvokeHTTP>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTReceiver>();
    LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();
    LogTestController::getInstance().setTrace<minifi::coap::c2::CoapProtocol>();
    LogTestController::getInstance().setOff<processors::LogAttribute>();
    LogTestController::getInstance().setOff<minifi::core::ProcessSession>();
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

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    assert(LogTestController::getInstance().contains("Received ack. version 3. number of operations 1") == true);
    assert(LogTestController::getInstance().contains("Received ack. version 3. number of operations 0") == true);
    assert(LogTestController::getInstance().contains("Received error event from protocol") == true);
    assert(LogTestController::getInstance().contains("Received op 1, with id id and operand operand") == true);
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
    uint16_t newport = std::stoi(port) + 2;
    auto new_port_str = std::to_string(newport);

    server = std::unique_ptr<minifi::coap::CoapServer>(new minifi::coap::CoapServer("127.0.0.1", newport));


    server->add_endpoint(minifi::coap::METHOD::POST, [](minifi::coap::CoapQuery)->minifi::coap::CoapResponse {
      minifi::coap::CoapResponse response(205,0x00,0);
      return response;

    });

    {
      // valid response version 3, 0 ops
      uint8_t *data = new uint8_t[5] { 0x00, 0x03, 0x00, 0x01, 0x00 };
      minifi::coap::CoapResponse response(205, std::unique_ptr<uint8_t>(data), 5);
      responses.enqueue(std::move(response));
    }

    {
      // valid response
      uint8_t *data = new uint8_t[5] { 0x00, 0x03, 0x00, 0x00, 0x00 };
      minifi::coap::CoapResponse response(205, std::unique_ptr<uint8_t>(data), 5);
      responses.enqueue(std::move(response));
    }

    {
      // should result in valid operation
      minifi::io::BaseStream stream;
      uint16_t version = 0, size = 1;
      uint8_t operation = 1;
      stream.write(version);
      stream.write(size);
      stream.write(&operation, 1);
      stream.writeUTF("id");
      stream.writeUTF("operand");

      uint8_t *data = new uint8_t[stream.getSize()];
      memcpy(data, stream.getBuffer(), stream.getSize());
      minifi::coap::CoapResponse response(205, std::unique_ptr<uint8_t>(data), stream.getSize());
      responses.enqueue(std::move(response));
    }

    server->add_endpoint("heartbeat", minifi::coap::METHOD::POST, [&](minifi::coap::CoapQuery)-> minifi::coap::CoapResponse {
      if (responses.size_approx() > 0) {
        minifi::coap::CoapResponse resp(500,0,0);;
        responses.try_dequeue(resp);
        return resp;
      }
      else {
        minifi::coap::CoapResponse response(500,0,0);
        return response;
      }

    });
    server->start();
    configuration->set("c2.enable", "true");
    configuration->set("c2.agent.class", "test");
    configuration->set("nifi.c2.root.classes", "DeviceInfoNode,AgentInformation,FlowInformation,RepositoryMetrics");
    configuration->set("nifi.c2.agent.protocol.class", "CoapProtocol");
    configuration->set("nifi.c2.agent.coap.host", "127.0.0.1");
    configuration->set("nifi.c2.agent.coap.port", new_port_str);
    configuration->set("c2.agent.heartbeat.period", "10");
    configuration->set("c2.rest.listener.heartbeat.rooturi", path);
  }

 protected:
  moodycamel::ConcurrentQueue<minifi::coap::CoapResponse> responses;
  std::unique_ptr<minifi::coap::CoapServer> server;
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

  VerifyCoAPServer harness(isSecure);

  harness.setKeyDir(key_dir);

  harness.run(test_file_location);

  return 0;
}
