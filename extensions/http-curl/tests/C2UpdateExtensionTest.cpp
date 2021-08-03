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
#include <vector>
#include <string>
#include <fstream>
#include <iterator>

#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "utils/IntegrationTestUtils.h"
#include "CivetStream.h"
#include "StreamPipe.h"
#include "OptionalUtils.h"
#include "utils/Environment.h"

static const std::string expected_content = "hello from file";

class FileProvider : public ServerAwareHandler {
 public:
  bool handleGet(CivetServer* /*server*/, struct mg_connection* conn) override {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                    "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              expected_content.length());
    mg_printf(conn, "%s", expected_content.c_str());
    return true;
  }
};

class C2HeartbeatHandler : public ServerAwareHandler {
 public:
  explicit C2HeartbeatHandler(std::string response) : response_(std::move(response)) {}

  bool handlePost(CivetServer* /*server*/, struct mg_connection* conn) override {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                    "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              response_.length());
    mg_printf(conn, "%s", response_.c_str());
    return true;
  }

 private:
  std::string response_;
};

class VerifyC2ExtensionUpdate : public VerifyC2Base {
 public:
  explicit VerifyC2ExtensionUpdate(std::function<bool()> verify) : verify_(std::move(verify)) {}

  void configureC2() override {
    configuration->set("nifi.c2.agent.protocol.class", "RESTSender");
    configuration->set("nifi.c2.enable", "true");
    configuration->set("nifi.c2.agent.heartbeat.period", "100");
    configuration->set("nifi.c2.root.classes", "DeviceInfoNode,AgentInformation,FlowInformation");
  }

  void runAssertions() override {
    assert(utils::verifyEventHappenedInPollTime(std::chrono::seconds(3000), verify_));
  }

 private:
  std::function<bool()> verify_;
};

std::string readFile(const std::filesystem::path& file) {
  std::ifstream stream{file};
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

int main() {
  TestController controller;

  // setup executable directory
  const std::filesystem::path root_dir = controller.createTempDirectory();
  utils::file::test_set_mock_executable_path(root_dir / "bin" / "minifi.exe");
  auto extension_dir = utils::file::get_extension_dir();

  // setup minifi home
  const std::filesystem::path home_dir = controller.createTempDirectory();
  utils::Environment::setCurrentWorkingDirectory(home_dir.c_str());
  utils::Environment::setEnvironmentVariable(MINIFI_HOME_ENV_KEY, home_dir.c_str());

  const std::string file_update_id = "321";

  C2AcknowledgeHandler ack_handler;
  FileProvider file_provider;

  VerifyC2ExtensionUpdate harness([&]() -> bool {
    if (!ack_handler.isAcknowledged(file_update_id)) {
      return false;
    }
    std::string file1_content = readFile(home_dir / "example.txt");
    assert(file1_content == expected_content);

    std::string file2_content = readFile(home_dir / "new_dir" / "renamed.txt");
    assert(file2_content == expected_content);

    std::string file3_content = readFile(extension_dir / "rel_to_binary.txt");
    assert(file3_content == expected_content);

    std::string file4_content = readFile(extension_dir / "also_new_dir" / "rel_to_binary.txt.sig");
    assert(file4_content == expected_content);

    return true;
  });
  harness.setUrl("http://localhost:0/api/file/example.txt", &file_provider);
  // server is initialized after this, we can use the port
  std::string file_url = "http://localhost:" + harness.getWebPort() + "/api/file/example.txt";
  C2HeartbeatHandler heartbeat_handler(R"({
    "requested_operations": [{
      "operation": "update",
      "name": "extension",
      "operationId": ")" + file_update_id + R"(",
      "args": {
        "${MINIFI_HOME}/example.txt": ")" + file_url + R"(",
        "${MINIFI_HOME}/new_dir/renamed.txt": ")" + file_url + R"(",
        "rel_to_binary.txt": ")" + file_url + R"(",
        "also_new_dir/rel_to_binary.txt.sig": ")" + file_url + R"("
      }
    }]})");
  harness.setUrl("http://localhost:0/api/heartbeat", &heartbeat_handler);
  harness.setUrl("http://localhost:0/api/acknowledge", &ack_handler);
  harness.setC2Url("/api/heartbeat", "/api/acknowledge");

  harness.run();
}
