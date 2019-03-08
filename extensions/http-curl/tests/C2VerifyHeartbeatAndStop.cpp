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
#include "c2/C2Agent.h"
#include "protocols/RESTReceiver.h"
#include "protocols/RESTSender.h"
#include "HTTPIntegrationBase.h"
#include "agent/build_description.h"
#include "processors/LogAttribute.h"

class Responder : public CivetHandler {
 public:
  explicit Responder(bool isSecure)
      : isSecure(isSecure) {
  }

  std::string readPost(struct mg_connection *conn) {
    std::string response;
    int blockSize = 1024 * sizeof(char), readBytes;

    char buffer[blockSize];
    while ((readBytes = mg_read(conn, buffer, blockSize)) > 0) {
      response.append(buffer, 0, (readBytes / sizeof(char)));
    }
    return response;
  }
  bool handlePost(CivetServer *server, struct mg_connection *conn) {
    auto post_data = readPost(conn);

    std::cerr << post_data << std::endl;

    if (!IsNullOrEmpty(post_data)) {
      rapidjson::Document root;
      rapidjson::ParseResult ok = root.Parse(post_data.data(), post_data.size());
      bool found = false;
      std::string operation = root["operation"].GetString();
      if (operation == "heartbeat") {
        assert(root.HasMember("agentInfo") == true);
        assert(root["agentInfo"]["agentManifest"].HasMember("bundles") == true);

        for (auto &bundle : root["agentInfo"]["agentManifest"]["bundles"].GetArray()) {
          assert(bundle.HasMember("artifact"));
          std::string str = bundle["artifact"].GetString();
          if (str == "minifi-system") {

            std::vector<std::string> classes;
            for (auto &proc : bundle["componentManifest"]["processors"].GetArray()) {
              classes.push_back(proc["type"].GetString());
            }

            auto group = minifi::BuildDescription::getClassDescriptions(str);
            for (auto proc : group.processors_) {
              assert(std::find(classes.begin(), classes.end(), proc.class_name_) != std::end(classes));
              found = true;
            }

          }
        }
        assert(found == true);
      }
    }
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

class VerifyC2Heartbeat : public CoapIntegrationBase {
 public:
  explicit VerifyC2Heartbeat(bool isSecure)
      : isSecure(isSecure) {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  void testSetup() {
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<LogTestController>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTProtocol>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTReceiver>();
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
    assert(LogTestController::getInstance().contains("Received Ack from Server") == true);

    assert(LogTestController::getInstance().contains("C2Agent] [debug] Stopping component invoke") == true);

    assert(LogTestController::getInstance().contains("C2Agent] [debug] Stopping component FlowController") == true);
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) {
    std::shared_ptr<core::Processor> proc = pg->findProcessor("invoke");
    assert(proc != nullptr);

    std::shared_ptr<minifi::processors::InvokeHTTP> inv = std::dynamic_pointer_cast<minifi::processors::InvokeHTTP>(proc);

    assert(inv != nullptr);
    std::string url = "";
    inv->getProperty(minifi::processors::InvokeHTTP::URL.getName(), url);

    configuration->set("nifi.c2.agent.protocol.class", "RESTSender");
    configuration->set("nifi.c2.enable", "true");
    configuration->set("nifi.c2.agent.class", "test");
    configuration->set("nifi.c2.rest.url", "http://localhost:8888/api/heartbeat");
    configuration->set("nifi.c2.agent.heartbeat.period", "1000");
    configuration->set("nifi.c2.rest.url.ack", "http://localhost:8888/api/heartbeat");
    configuration->set("nifi.c2.root.classes", "DeviceInfoNode,AgentInformation,FlowInformation");
  }

 protected:
  bool isSecure;
  char *dir;
  std::stringstream ss;
  TestController testController;
};

int main(int argc, char **argv) {
  std::string key_dir, test_file_location, url;
  url = "http://localhost:8888/api/heartbeat";
  if (argc > 1) {
    test_file_location = argv[1];
    key_dir = argv[2];
  }

  bool isSecure = false;
  if (url.find("https") != std::string::npos) {
    isSecure = true;
  }

  VerifyC2Heartbeat harness(isSecure);

  harness.setKeyDir(key_dir);

  Responder responder(isSecure);

  harness.setUrl(url, &responder);

  harness.run(test_file_location);

  return 0;
}
