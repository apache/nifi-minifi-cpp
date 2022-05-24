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
#include "TestBase.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "processors/TailFile.h"
#include "state/ProcessorController.h"
#include "utils/file/FileUtils.h"
#include "utils/TestUtils.h"
#include "processors/GetTCP.h"

using namespace std::literals::chrono_literals;

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
  explicit MetricsHandler(std::atomic_bool& metrics_updated_successfully, std::shared_ptr<minifi::Configure> configuration)
    : HeartbeatHandler(std::move(configuration)),
      metrics_updated_successfully_(metrics_updated_successfully) {
  }

  void handleHeartbeat(const rapidjson::Document& root, struct mg_connection* conn) override {
    switch (test_state_) {
      case TestState::VERIFY_INITIAL_METRICS: {
        verifyMetrics(root);
        sendEmptyHeartbeatResponse(conn);
        break;
      }
      case TestState::SEND_NEW_CONFIG: {
        sendHeartbeatResponse("UPDATE", "configuration", "889348", conn,
          {{"configuration_data", "Flow Controller:\\n  name: MiNiFi Flow\\nProcessors: []\\nConnections: []\\nRemote Processing Groups: []\\nProvenance Reporting:"}});
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
      verifyEmptyRuntimeMetrics(root["metrics"]["RuntimeMetrics"]) &&
      verifyLoadMetrics(root["metrics"]["LoadMetrics"]);

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

  bool verifyEmptyRuntimeMetrics(const rapidjson::Value& runtime_metrics) {
    return runtime_metrics.HasMember("deviceInfo") &&
      runtime_metrics.HasMember("flowInfo") &&
      runtime_metrics["flowInfo"].HasMember("versionedFlowSnapshotURI") &&
      runtime_metrics["flowInfo"].HasMember("components") &&
      runtime_metrics["flowInfo"]["components"].HasMember("FlowController") &&
      !runtime_metrics["flowInfo"].HasMember("queues") &&
      !runtime_metrics["flowInfo"]["components"].HasMember("GetTCP") &&
      !runtime_metrics["flowInfo"]["components"].HasMember("LogAttribute");
  }

  bool verifyLoadMetrics(const rapidjson::Value& load_metrics) {
    return load_metrics.HasMember("RepositoryMetrics") &&
      load_metrics["RepositoryMetrics"].HasMember("ff") &&
      load_metrics["RepositoryMetrics"].HasMember("repo_name");
  }

  bool verifyProcessorMetrics(const rapidjson::Value& processor_metrics) {
    return processor_metrics.HasMember("GetTCPMetrics") &&
      processor_metrics["GetTCPMetrics"].HasMember("OnTriggerInvocations") &&
      processor_metrics["GetTCPMetrics"]["OnTriggerInvocations"].GetUint() > 0;
  }

  std::atomic_bool& metrics_updated_successfully_;
  TestState test_state_ = TestState::VERIFY_INITIAL_METRICS;
};

int main(int argc, char **argv) {
  std::atomic_bool metrics_updated_successfully{false};
  const cmd_args args = parse_cmdline_args(argc, argv, "api/heartbeat");
  VerifyC2Metrics harness(metrics_updated_successfully);
  harness.getConfiguration()->set("nifi.c2.root.class.definitions", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.name", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics", "runtimemetrics,loadmetrics,processorMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.runtimemetrics.name", "RuntimeMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.runtimemetrics.classes", "DeviceInfoNode,FlowInformation");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.loadmetrics.name", "LoadMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.loadmetrics.classes", "RepositoryMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processorMetrics.name", "ProcessorMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processorMetrics.classes", "GetTCPMetrics");
  harness.setKeyDir(args.key_dir);
  MetricsHandler handler(metrics_updated_successfully, harness.getConfiguration());
  harness.setUrl(args.url, &handler);
  harness.run(args.test_file);
  return 0;
}
