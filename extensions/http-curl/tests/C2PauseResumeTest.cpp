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

#include <memory>

#include "TestBase.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "InvokeHTTP.h"
#include "TestServer.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "io/StreamFactory.h"
#include "integration/IntegrationBase.h"

class VerifyC2PauseResume : public VerifyC2Base {
 public:
  explicit VerifyC2PauseResume(const std::atomic_bool& flow_resumed_successfully) : VerifyC2Base(), flow_resumed_successfully_(flow_resumed_successfully) {}

  void configureC2() override {
    VerifyC2Base::configureC2();
    configuration->set("nifi.c2.agent.heartbeat.period", "500");
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(std::chrono::seconds(20), [&] { return flow_resumed_successfully_.load(); }));
  }

 private:
  const std::atomic_bool& flow_resumed_successfully_;
};

class PauseResumeHandler: public HeartbeatHandler {
 public:
  static const uint32_t PAUSE_SECONDS = 3;
  static const uint32_t INITIAL_GET_INVOKE_COUNT = 2;

  explicit PauseResumeHandler(std::atomic_bool& flow_resumed_successfully) : HeartbeatHandler(), flow_resumed_successfully_(flow_resumed_successfully) {}
  bool handleGet(CivetServer* /*server*/, struct mg_connection* conn) override {
    assert(flow_state_ != FlowState::PAUSED);
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
    } else if (get_invoke_count_ == INITIAL_GET_INVOKE_COUNT && flow_state_ == FlowState::STARTED) {
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
  std::chrono::time_point<std::chrono::system_clock> pause_start_time_;
  std::atomic<FlowState> flow_state_{FlowState::STARTED};
  std::atomic_bool& flow_resumed_successfully_;
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv, "heartbeat");
  std::atomic_bool flow_resumed_successfully{false};
  VerifyC2PauseResume harness{flow_resumed_successfully};
  harness.setKeyDir(args.key_dir);
  PauseResumeHandler responder{flow_resumed_successfully};

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  configuration->set(minifi::Configure::nifi_default_directory, args.key_dir);
  configuration->set(minifi::Configure::nifi_flow_configuration_file, args.test_file);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(configuration);

  std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::make_unique<core::YamlConfiguration>(
    test_repo, test_repo, content_repo, stream_factory, configuration, args.test_file);

  std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(
      test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo, DEFAULT_ROOT_GROUP_NAME);

  core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory, configuration, args.test_file);

  std::shared_ptr<core::Processor> proc = yaml_config.getRoot()->findProcessorByName("invoke");
  assert(proc != nullptr);

  const auto inv = std::dynamic_pointer_cast<minifi::processors::InvokeHTTP>(proc);
  assert(inv != nullptr);
  std::string url;
  inv->getProperty(minifi::processors::InvokeHTTP::URL.getName(), url);
  std::string port, scheme, path;
  std::unique_ptr<TestServer> server;
  parse_http_components(url, port, scheme, path);
  server = std::make_unique<TestServer>(port, path, &responder);

  harness.setUrl(args.url, &responder);
  harness.run(args.test_file);
}
