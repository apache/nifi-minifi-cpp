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

void waitToVerifyProcessor() {
  std::this_thread::sleep_for(std::chrono::seconds(10));
}

static std::vector<std::string> responses;

class ConfigHandler : public CivetHandler {
 public:
  ConfigHandler() {
    calls_ = 0;
  }
  virtual bool handlePost(CivetServer *server, struct mg_connection *conn) override {
    calls_++;
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    long long remainlen;
    long long readlen = 0;
    long long contentlen = req_info->content_length;
    char buf[1024];

    std::string data;
    while (readlen < contentlen) {
      remainlen = contentlen - readlen;
      if (remainlen > sizeof(buf)) {
        remainlen = sizeof(buf);
      }
      remainlen = mg_read(conn, buf, (size_t) remainlen);
      if (remainlen <= 0) {
        break;
      }
      readlen += remainlen;
      data += std::string(buf, remainlen);
    }
    if (data.find("operationState") != std::string::npos) {
      assert(data.find("state\": \"NOT_APPLIED") != std::string::npos);
    }

    if (responses.size() > 0) {
      std::string top_str = responses.back();
      responses.pop_back();
      mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
                top_str.length());
      mg_printf(conn, "%s", top_str.c_str());
    } else {
      mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\n");
    }

    return true;
  }

  virtual bool handleGet(CivetServer *server, struct mg_connection *conn) override {
    std::ifstream myfile(test_file_location_.c_str());

    if (myfile.is_open()) {
      std::stringstream buffer;
      buffer << myfile.rdbuf();
      std::string str = buffer.str();
      myfile.close();
      mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
                str.length());
      mg_printf(conn, "%s", str.c_str());
    } else {
      mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\n");
    }

    return true;
  }
  std::string test_file_location_;
  std::string base_location_;
  std::atomic<size_t> calls_;
};

int main(int argc, char **argv) {
  mg_init_library(0);
  LogTestController::getInstance().setInfo<minifi::FlowController>();
  LogTestController::getInstance().setDebug<minifi::utils::HTTPClient>();
  LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
  LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();

  const char *options[] = { "document_root", ".", "listening_ports", "7071", 0 };
  std::vector<std::string> cpp_options;
  for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
    cpp_options.push_back(options[i]);
  }

  CivetServer server(cpp_options);
  ConfigHandler h_ex;
  server.addHandler("/update", h_ex);
  std::string key_dir, test_file_location;
  if (argc > 1) {
    h_ex.base_location_ = test_file_location = argv[1];
    h_ex.test_file_location_ = argv[2];
    key_dir = argv[3];
  }
  std::string heartbeat_response = "{\"operation\" : \"heartbeat\",\"requested_operations\": [  {"
      "\"operation\" : \"update\", "
      "\"operationid\" : \"8675309\", "
      "\"name\": \"configuration\""
      "}]}";

  responses.push_back(heartbeat_response);

  std::ifstream myfile(test_file_location.c_str());

  if (myfile.is_open()) {
    std::stringstream buffer;
    buffer << myfile.rdbuf();
    std::string str = buffer.str();
    myfile.close();
    std::string response = "{\"operation\" : \"heartbeat\",\"requested_operations\": [  {"
        "\"operation\" : \"update\", "
        "\"operationid\" : \"8675309\", "
        "\"name\": \"configuration\", \"content\": { \"location\": \"http://localhost:7071/update\"}}]}";
    responses.push_back(response);
  }

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();

  configuration->set("nifi.c2.agent.protocol.class", "RESTSender");
  configuration->set("nifi.c2.enable", "true");
  configuration->set("nifi.c2.agent.class", "test");
  configuration->set("nifi.c2.rest.url", "http://localhost:7071/update");
  configuration->set("nifi.c2.rest.url.ack", "http://localhost:7071/update");
  configuration->set("nifi.c2.agent.heartbeat.period", "1000");
  mkdir("content_repository", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file, test_file_location);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<core::YamlConfiguration>(
      new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location));
  std::shared_ptr<TestRepository> repo = std::static_pointer_cast<TestRepository>(test_repo);

  std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo, DEFAULT_ROOT_GROUP_NAME,
                                                                                                true);

  core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location);

  std::unique_ptr<core::ProcessGroup> ptr = yaml_config.getRoot(test_file_location);
  std::shared_ptr<core::ProcessGroup> pg = std::shared_ptr<core::ProcessGroup>(ptr.get());
  ptr.release();
  auto start = std::chrono::system_clock::now();

  controller->load();
  controller->start();
  waitToVerifyProcessor();

  controller->waitUnload(60000);
  auto then = std::chrono::system_clock::now();

  auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(then - start).count();
  std::string logs = LogTestController::getInstance().log_output.str();
  assert(logs.find("Invalid configuration payload") != std::string::npos);
  assert(logs.find("update failed.") != std::string::npos);
  LogTestController::getInstance().reset();
  rmdir("./content_repository");
  assert(h_ex.calls_ <= (milliseconds / 1000) + 1);

  return 0;
}
