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
#include <memory>

#include "unit/TestBase.h"
#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "processors/InvokeHTTP.h"
#include "integration/TestServer.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "integration/IntegrationBase.h"
#include "unit/Catch.h"
#include "unit/ProvenanceTestHelper.h"

namespace org::apache::nifi::minifi::test {

class VerifyC2PauseResume : public VerifyC2Base {
 public:
  explicit VerifyC2PauseResume(const std::atomic_bool& flow_resumed_successfully) : VerifyC2Base(), flow_resumed_successfully_(flow_resumed_successfully) {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessContext>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
  }

  void configureC2() override {
    VerifyC2Base::configureC2();
    configuration->set(minifi::Configuration::nifi_c2_agent_heartbeat_period, "500");
  }

  void runAssertions() override {
    REQUIRE(utils::verifyEventHappenedInPollTime(std::chrono::seconds(20), [&] { return flow_resumed_successfully_.load(); }));
  }

 private:
  const std::atomic_bool& flow_resumed_successfully_;
};

class PauseResumeHandler: public HeartbeatHandler {
 public:
  static const uint32_t PAUSE_SECONDS = 3;
  static const uint32_t INITIAL_GET_INVOKE_COUNT = 2;

  explicit PauseResumeHandler(std::atomic_bool& flow_resumed_successfully, std::shared_ptr<minifi::Configure> configuration)
    : HeartbeatHandler(std::move(configuration)), flow_resumed_successfully_(flow_resumed_successfully) {
  }
  bool handleGet(CivetServer* /*server*/, struct mg_connection* conn) override {
    REQUIRE(flow_state_ != FlowState::PAUSED);
    ++get_invoke_count_;
    if (flow_state_ == FlowState::RESUMED) {
      flow_resumed_successfully_ = true;
    }

    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
    return true;
  }

  void handleHeartbeat(const rapidjson::Document&, struct mg_connection * conn) override {
    std::string operation = "resume";
    if (flow_state_ == FlowState::PAUSE_INITIATED) {
      pause_start_time_ = std::chrono::system_clock::now();
      flow_state_ = FlowState::PAUSED;
      operation = "pause";
    } else if (get_invoke_count_ >= INITIAL_GET_INVOKE_COUNT && flow_state_ == FlowState::STARTED) {
      flow_state_ = FlowState::PAUSE_INITIATED;
      operation = "pause";
    } else if (flow_state_ == FlowState::PAUSED) {
      operation = "pause";
    }

    std::string heartbeat_response = "{\"operation\" : \"heartbeat\",\"requested_operations\": [  {"
          "\"operation\" : \"" + operation + "\","
          "\"operationid\" : \"8675309\"}]}";

    if (flow_state_ == FlowState::PAUSED && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - pause_start_time_).count() > PAUSE_SECONDS) {
      flow_state_ = FlowState::RESUMED;
    }

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
              "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              heartbeat_response.length());
    mg_printf(conn, "%s", heartbeat_response.c_str());
  }

 private:
  enum class FlowState {
    STARTED,
    PAUSE_INITIATED,
    PAUSED,
    RESUMED
  };

  std::atomic<uint32_t> get_invoke_count_{0};
  std::chrono::system_clock::time_point pause_start_time_;
  std::atomic<FlowState> flow_state_{FlowState::STARTED};
  std::atomic_bool& flow_resumed_successfully_;
};

TEST_CASE("C2PauseResumeTest", "[c2test]") {
  std::atomic_bool flow_resumed_successfully{false};
  VerifyC2PauseResume harness{flow_resumed_successfully};
  PauseResumeHandler responder{flow_resumed_successfully, harness.getConfiguration()};

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestThreadedRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "C2PauseResumeTest.yml";
  configuration->set(minifi::Configure::nifi_flow_configuration_file, test_file_path.string());

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(configuration);

  auto yaml_ptr = std::make_shared<core::YamlConfiguration>(core::ConfigurationContext{
      .flow_file_repo = test_repo,
      .content_repo = content_repo,
      .configuration = configuration,
      .path = test_file_path,
      .filesystem = std::make_shared<minifi::utils::file::FileSystem>(),
      .sensitive_values_encryptor = minifi::utils::crypto::EncryptionProvider{minifi::utils::crypto::XSalsa20Cipher{minifi::utils::crypto::XSalsa20Cipher::generateKey()}}
  });
  std::vector<std::shared_ptr<core::RepositoryMetricsSource>> repo_metric_sources{test_repo, test_flow_repo, content_repo};
  auto metrics_publisher_store = std::make_unique<minifi::state::MetricsPublisherStore>(configuration, repo_metric_sources, yaml_ptr);
  std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration,
      std::move(yaml_ptr), content_repo, std::move(metrics_publisher_store));

  core::YamlConfiguration yaml_config(core::ConfigurationContext{
    .flow_file_repo = test_repo,
    .content_repo = content_repo,
    .configuration = configuration,
    .path = test_file_path,
    .filesystem = std::make_shared<minifi::utils::file::FileSystem>(),
    .sensitive_values_encryptor = minifi::utils::crypto::EncryptionProvider{minifi::utils::crypto::XSalsa20Cipher{minifi::utils::crypto::XSalsa20Cipher::generateKey()}}
  });
  auto root = yaml_config.getRoot();
  const auto proc = root->findProcessorByName("invoke");
  REQUIRE(proc != nullptr);

  const auto inv = dynamic_cast<minifi::processors::InvokeHTTP*>(proc);
  REQUIRE(inv != nullptr);
  std::string url;
  inv->getProperty(minifi::processors::InvokeHTTP::URL, url);
  std::string port;
  std::string scheme;
  std::string path;
  std::unique_ptr<TestServer> server;
  minifi::utils::parse_http_components(url, port, scheme, path);
  server = std::make_unique<TestServer>(port, path, &responder);

  harness.setUrl("http://localhost:0/heartbeat", &responder);
  harness.run(test_file_path);
}

}  // namespace org::apache::nifi::minifi::test
