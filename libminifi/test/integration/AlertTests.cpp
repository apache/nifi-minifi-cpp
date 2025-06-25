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
#include "unit/TestBase.h"
#include "CivetServer.h"
#include "integration/ServerAwareHandler.h"
#include "integration/TestServer.h"
#include "integration/HTTPIntegrationBase.h"
#include "rapidjson/document.h"
#include "unit/EmptyFlow.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

class AlertHandler : public ServerAwareHandler {
 public:
  explicit AlertHandler(std::string agent_id): agent_id_(std::move(agent_id)) {}

  bool handlePut(CivetServer* , struct mg_connection *conn) override {
    auto msg = readPayload(conn);
    rapidjson::Document doc;
    rapidjson::ParseResult res = doc.Parse(msg.c_str());
    REQUIRE(static_cast<bool>(res));
    REQUIRE(doc.IsObject());
    REQUIRE(doc.HasMember("agentId"));
    REQUIRE(doc["agentId"].IsString());
    REQUIRE(doc.HasMember("alerts"));
    REQUIRE(doc["alerts"].IsArray());
    REQUIRE(doc["alerts"].Size() > 0);
    std::string id(doc["agentId"].GetString(), doc["agentId"].GetStringLength());
    REQUIRE(id == agent_id_);
    std::vector<std::string> batch;
    for (rapidjson::SizeType i = 0; i < doc["alerts"].Size(); ++i) {
      REQUIRE(doc["alerts"][i].IsString());
      batch.emplace_back(doc["alerts"][i].GetString(), doc["alerts"][i].GetStringLength());
    }
    alerts_.enqueue(std::move(batch));
    return true;
  }

  std::string agent_id_;
  minifi::utils::ConditionConcurrentQueue<std::vector<std::string>> alerts_;
};

class VerifyAlerts : public HTTPIntegrationBase {
 public:
  void testSetup() override {}

  void runAssertions() override {
    verify_();
  }

  std::function<bool()> verify_;
};

TEST_CASE("Alert system forwards logs") {
  auto clock = std::make_shared<minifi::test::utils::ManualClock>();
  minifi::utils::timeutils::setClock(clock);

  TempDirectory dir;
  auto flow_config_file = dir.getPath() / "config.yml";
  std::ofstream(flow_config_file) << empty_flow;

  std::string agent_id = "test-agent-1";
  VerifyAlerts harness;
  AlertHandler handler(agent_id);
  harness.setUrl("http://localhost:0/api/alerts", &handler);
  harness.getConfiguration()->set(minifi::Configuration::nifi_c2_agent_identifier, agent_id);
  harness.getConfiguration()->setLocations(minifi::LocationsImpl::createFromMinifiHome(dir.getPath()));

  auto log_props = std::make_shared<logging::LoggerProperties>(dir.getPath() / "logs");
  log_props->set("appender.alert1", "alert");
  log_props->set("appender.alert1.url", harness.getC2RestUrl());
  log_props->set("appender.alert1.filter", ".*<begin>(.*)<end>.*");
  log_props->set("appender.alert1.rate.limit", "10 s");
  log_props->set("appender.alert1.flush.period", "1 s");
  log_props->set("logger.root", "INFO,alert1");
  logging::LoggerConfiguration::getConfiguration().initialize(log_props);

  auto verifyLogsArrived = [&] (const std::vector<std::string>& expected) {
    std::vector<std::string> logs;
    REQUIRE(handler.alerts_.dequeueWaitFor(logs, 1s));
    REQUIRE(logs.size() == expected.size());
    for (size_t idx = 0; idx < expected.size(); ++idx) {
      bool contains = std::search(logs[idx].begin(), logs[idx].end(), expected[idx].begin(), expected[idx].end()) != logs[idx].end();
      REQUIRE(contains);
    }
  };

  harness.verify_ = [&] {
    auto logger = logging::LoggerFactory<minifi::FlowController>::getLogger();
    // time = 0
    logger->log_error("not matched");
    logger->log_error("<begin>one<end>");
    logger->log_error("not the same but treated so <begin>one<end>");
    logger->log_error("<begin>two<end>");
    clock->advance(2s);
    // time = 2
    verifyLogsArrived({
        "<begin>one<end>", "<begin>two<end>"
    });

    clock->advance(5s);
    // time = 7
    // no new logs over HTTP

    logger->log_error("other <begin>one<end>");
    logger->log_error("new log <begin>three<end>");
    clock->advance(2s);

    // time = 9
    verifyLogsArrived({
        "new log <begin>three<end>"
    });

    clock->advance(2s);
    // time = 11
    logger->log_error("other <begin>one<end>");
    logger->log_error("new log <begin>three<end>");
    clock->advance(2s);
    // time = 13

    verifyLogsArrived({
        "other <begin>one<end>"
    });

    return true;
  };

  harness.run(flow_config_file.string(), dir.getPath());
}

}  // namespace org::apache::nifi::minifi::test
