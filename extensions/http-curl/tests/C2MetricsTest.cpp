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
#include <string>
#include <iostream>
#include <filesystem>

#include "TestBase.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "processors/TailFile.h"
#include "state/ProcessorController.h"
#include "utils/file/FileUtils.h"
#include "utils/TestUtils.h"
#include "processors/GetTCP.h"
#include "utils/StringUtils.h"
#include "utils/file/PathUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class VerifyC2Metrics : public VerifyC2Base {
 public:
  explicit VerifyC2Metrics(const std::atomic_bool& metrics_updated_successfully) : metrics_updated_successfully_(metrics_updated_successfully) {
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setOff<minifi::processors::GetTCP>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(40s, [&] { return metrics_updated_successfully_.load(); }, 1s));
  }

 private:
  const std::atomic_bool& metrics_updated_successfully_;
};

class MetricsHandler: public HeartbeatHandler {
 public:
  explicit MetricsHandler(std::atomic_bool& metrics_updated_successfully, std::shared_ptr<minifi::Configure> configuration, const std::string& replacement_config_path)
    : HeartbeatHandler(std::move(configuration)),
      metrics_updated_successfully_(metrics_updated_successfully),
      replacement_config_(getReplacementConfigAsJsonValue(replacement_config_path)) {
  }

  void handleHeartbeat(const rapidjson::Document& root, struct mg_connection* conn) override {
    switch (test_state_) {
      case TestState::VERIFY_INITIAL_METRICS: {
        verifyMetrics(root);
        sendEmptyHeartbeatResponse(conn);
        break;
      }
      case TestState::SEND_NEW_CONFIG: {
        sendHeartbeatResponse("UPDATE", "configuration", "889348", conn, {{"configuration_data", replacement_config_}});
        test_state_ = TestState::VERIFY_UPDATED_METRICS;
        break;
      }
      case TestState::VERIFY_UPDATED_METRICS: {
        verifyUpdatedMetrics(root);
        sendEmptyHeartbeatResponse(conn);
        break;
      }
    }
  }

 private:
  enum class TestState {
    VERIFY_INITIAL_METRICS,
    SEND_NEW_CONFIG,
    VERIFY_UPDATED_METRICS
  };

  void sendEmptyHeartbeatResponse(struct mg_connection* conn) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
  }

  void verifyMetrics(const rapidjson::Document& root) {
    auto initial_metrics_verified =
      root.HasMember("metrics") &&
      root["metrics"].HasMember("RuntimeMetrics") &&
      root["metrics"].HasMember("LoadMetrics") &&
      root["metrics"].HasMember("ProcessorMetrics") &&
      verifyRuntimeMetrics(root["metrics"]["RuntimeMetrics"]) &&
      verifyLoadMetrics(root["metrics"]["LoadMetrics"]) &&
      verifyProcessorMetrics(root["metrics"]["ProcessorMetrics"]);

    if (initial_metrics_verified) {
      test_state_ = TestState::SEND_NEW_CONFIG;
    }
  }

  void verifyUpdatedMetrics(const rapidjson::Document& root) {
    auto updated_metrics_verified =
      root.HasMember("metrics") &&
      root["metrics"].HasMember("RuntimeMetrics") &&
      root["metrics"].HasMember("LoadMetrics") &&
      !root["metrics"].HasMember("ProcessorMetrics") &&
      verifyUpdatedRuntimeMetrics(root["metrics"]["RuntimeMetrics"]) &&
      verifyUpdatedLoadMetrics(root["metrics"]["LoadMetrics"]);

    if (updated_metrics_verified) {
      metrics_updated_successfully_ = true;
    }
  }

  bool verifyRuntimeMetrics(const rapidjson::Value& runtime_metrics) {
    return runtime_metrics.HasMember("deviceInfo") &&
      runtime_metrics.HasMember("flowInfo") &&
      runtime_metrics["flowInfo"].HasMember("versionedFlowSnapshotURI") &&
      runtime_metrics["flowInfo"].HasMember("queues") &&
      runtime_metrics["flowInfo"].HasMember("components") &&
      runtime_metrics["flowInfo"]["queues"].HasMember("2438e3c8-015a-1000-79ca-83af40ec1997") &&
      runtime_metrics["flowInfo"]["components"].HasMember("FlowController") &&
      runtime_metrics["flowInfo"]["components"].HasMember("GetTCP") &&
      runtime_metrics["flowInfo"]["components"].HasMember("LogAttribute");
  }

  bool verifyUpdatedRuntimeMetrics(const rapidjson::Value& runtime_metrics) {
    return runtime_metrics.HasMember("deviceInfo") &&
      runtime_metrics.HasMember("flowInfo") &&
      runtime_metrics["flowInfo"].HasMember("versionedFlowSnapshotURI") &&
      runtime_metrics["flowInfo"].HasMember("queues") &&
      runtime_metrics["flowInfo"].HasMember("components") &&
      runtime_metrics["flowInfo"]["queues"].HasMember("8368e3c8-015a-1003-52ca-83af40ec1332") &&
      runtime_metrics["flowInfo"]["components"].HasMember("FlowController") &&
      runtime_metrics["flowInfo"]["components"].HasMember("GenerateFlowFile") &&
      runtime_metrics["flowInfo"]["components"].HasMember("LogAttribute");
  }

  bool verifyLoadMetrics(const rapidjson::Value& load_metrics) {
    return load_metrics.HasMember("RepositoryMetrics") &&
      load_metrics.HasMember("QueueMetrics") &&
      load_metrics["RepositoryMetrics"].HasMember("ff") &&
      load_metrics["RepositoryMetrics"].HasMember("repo_name") &&
      load_metrics["QueueMetrics"].HasMember("GetTCP/success/LogAttribute");
  }

  bool verifyUpdatedLoadMetrics(const rapidjson::Value& load_metrics) {
    return load_metrics.HasMember("RepositoryMetrics") &&
      load_metrics.HasMember("QueueMetrics") &&
      load_metrics["RepositoryMetrics"].HasMember("ff") &&
      load_metrics["RepositoryMetrics"].HasMember("repo_name") &&
      load_metrics["QueueMetrics"].HasMember("GenerateFlowFile/success/LogAttribute") &&
      std::stoi(load_metrics["QueueMetrics"]["GenerateFlowFile/success/LogAttribute"]["queued"].GetString()) > 0;
  }

  bool verifyProcessorMetrics(const rapidjson::Value& processor_metrics) {
    return processor_metrics.HasMember("GetTCPMetrics") &&
      processor_metrics["GetTCPMetrics"].HasMember("OnTriggerInvocations") &&
      processor_metrics["GetTCPMetrics"]["OnTriggerInvocations"].GetUint() > 0;
  }

  std::string getReplacementConfigAsJsonValue(const std::string& replacement_config_path) const {
    std::ifstream is(replacement_config_path);
    auto content = std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    content = utils::StringUtils::replaceAll(content, "\n", "\\n");
    content = utils::StringUtils::replaceAll(content, "\"", "\\\"");
    return content;
  }

  std::atomic_bool& metrics_updated_successfully_;
  TestState test_state_ = TestState::VERIFY_INITIAL_METRICS;
  std::string replacement_config_;
};

}  // namespace org::apache::nifi::minifi::test

int main(int argc, char **argv) {
  std::atomic_bool metrics_updated_successfully{false};
  const cmd_args args = parse_cmdline_args(argc, argv, "api/heartbeat");
  org::apache::nifi::minifi::test::VerifyC2Metrics harness(metrics_updated_successfully);
  harness.getConfiguration()->set("nifi.c2.root.class.definitions", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.name", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics", "runtimemetrics,loadmetrics,processorMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.runtimemetrics.name", "RuntimeMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.runtimemetrics.classes", "DeviceInfoNode,FlowInformation");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.loadmetrics.name", "LoadMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.loadmetrics.classes", "QueueMetrics,RepositoryMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processorMetrics.name", "ProcessorMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processorMetrics.classes", "GetTCPMetrics");
  harness.setKeyDir(args.key_dir);
  std::string path;
  std::string file;
  utils::file::getFileNameAndPath(args.test_file, path, file);
  std::string replacement_path = (std::filesystem::path{path} / "TestC2MetricsUpdate.yml").string();
  org::apache::nifi::minifi::test::MetricsHandler handler(metrics_updated_successfully, harness.getConfiguration(), replacement_path);
  harness.setUrl(args.url, &handler);
  harness.run(args.test_file);
  return 0;
}
