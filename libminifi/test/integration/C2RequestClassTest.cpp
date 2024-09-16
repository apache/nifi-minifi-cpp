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
#include <optional>
#include <string>
#include <vector>

#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "unit/TestUtils.h"
#include "integration/CivetStream.h"
#include "io/StreamPipe.h"
#include "properties/Configuration.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class C2HeartbeatHandler : public ServerAwareHandler {
 public:
  explicit C2HeartbeatHandler(std::string response) : response_(std::move(response)) {}

  bool handlePost(CivetServer* /*server*/, struct mg_connection* conn) override {
    std::string req = readPayload(conn);
    rapidjson::Document root;
    root.Parse(req.data(), req.size());
    std::optional<std::string> agent_class;
    if (root.IsObject() && root["agentInfo"].HasMember("agentClass")) {
      agent_class = root["agentInfo"]["agentClass"].GetString();
    }
    {
      std::lock_guard<std::mutex> lock(mtx_);
      classes_.push_back(agent_class);
    }

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                    "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              response_.length());
    mg_printf(conn, "%s", response_.c_str());
    return true;
  }

  bool gotClassesInOrder(const std::vector<std::optional<std::string>>& class_names) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = classes_.begin();
    for (const auto& class_name : class_names) {
      it = std::find(classes_.begin(), classes_.end(), class_name);
      if (it == classes_.end()) {
        return false;
      }
      ++it;
    }
    return true;
  }

 private:
  std::mutex mtx_;
  std::vector<std::optional<std::string>> classes_;
  std::string response_;
};

class VerifyC2ClassRequest : public VerifyC2Base {
 public:
  explicit VerifyC2ClassRequest(std::function<bool()> verify) : verify_(std::move(verify)) {}

  void configureC2() override {
    configuration->set(minifi::Configuration::nifi_c2_enable, "true");
    configuration->set(minifi::Configuration::nifi_c2_agent_heartbeat_period, "100");
    configuration->set(minifi::Configuration::nifi_c2_root_classes, "DeviceInfoNode,AgentInformation,FlowInformation");
  }

  void runAssertions() override {
    REQUIRE(utils::verifyEventHappenedInPollTime(std::chrono::seconds(10), verify_));
  }

 private:
  std::function<bool()> verify_;
};

TEST_CASE("C2RequestClassTest", "[c2test]") {
  const std::string class_update_id = "321";
  C2HeartbeatHandler heartbeat_handler(R"({
    "requested_operations": [{
      "operation": "update",
      "name": "properties",
      "operationId": ")" + class_update_id + R"(",
      "args": {
        "nifi.c2.agent.class": {"value": "TestClass", "persist": true}
      }
    }]})");
  C2AcknowledgeHandler ack_handler;

  VerifyC2ClassRequest harness([&]() -> bool {
    return heartbeat_handler.gotClassesInOrder({{}, {"TestClass"}}) &&
        ack_handler.isAcknowledged(class_update_id);
  });
  harness.setUrl("http://localhost:0/api/heartbeat", &heartbeat_handler);
  harness.setUrl("http://localhost:0/api/acknowledge", &ack_handler);
  harness.setC2Url("/api/heartbeat", "/api/acknowledge");

  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
