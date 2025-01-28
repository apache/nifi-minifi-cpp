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
#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>

#include "unit/TestBase.h"
#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "processors/TailFile.h"
#include "state/ProcessorController.h"
#include "utils/file/FileUtils.h"
#include "unit/TestUtils.h"
#include "processors/GetTCP.h"
#include "utils/StringUtils.h"
#include "utils/file/PathUtils.h"
#include "unit/Catch.h"

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
    using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
    REQUIRE(verifyEventHappenedInPollTime(40s, [&] { return metrics_updated_successfully_.load(); }, 1s));
  }

 private:
  const std::atomic_bool& metrics_updated_successfully_;
};

class MetricsHandler: public HeartbeatHandler {
 public:
  explicit MetricsHandler(std::atomic_bool& metrics_updated_successfully, std::shared_ptr<minifi::Configure> configuration, const std::filesystem::path& replacement_config_path)
    : HeartbeatHandler(std::move(configuration)),
      metrics_updated_successfully_(metrics_updated_successfully),
      replacement_config_(minifi::utils::file::get_content(replacement_config_path.string())) {
  }

  void handleHeartbeat(const rapidjson::Document& root, struct mg_connection* conn) override {
    switch (test_state_) {
      case TestState::VERIFY_INITIAL_METRICS: {
        verifyMetrics(root);
        sendEmptyHeartbeatResponse(conn);
        break;
      }
      case TestState::SEND_NEW_CONFIG: {
        sendHeartbeatResponse("UPDATE", "configuration", "889348", conn, {{"configuration_data", minifi::c2::C2Value{replacement_config_}}});
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

  static constexpr const char* GETTCP_UUID = "2438e3c8-015a-1000-79ca-83af40ec1991";
  static constexpr const char* LOGATTRIBUTE1_UUID = "2438e3c8-015a-1000-79ca-83af40ec1992";
  static constexpr const char* LOGATTRIBUTE2_UUID = "5128e3c8-015a-1000-79ca-83af40ec1990";
  static constexpr const char* GENERATE_FLOWFILE_UUID = "4fe2d51d-076a-49b0-88de-5cf5adf52b8f";

  static void sendEmptyHeartbeatResponse(struct mg_connection* conn) {
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

  static bool processorMetricsAreValid(const auto& processor) {
    return processor["bytesRead"].GetInt() >= 0 &&
      processor["bytesWritten"].GetInt() >= 0 &&
      processor["flowFilesIn"].GetInt() >= 0 &&
      processor["flowFilesOut"].GetInt() >= 0 &&
      processor["bytesIn"].GetInt() >= 0 &&
      processor["bytesOut"].GetInt() >= 0 &&
      processor["invocations"].GetInt() >= 0 &&
      processor["processingNanos"].GetInt() >= 0 &&
      processor["activeThreadCount"].GetInt() == -1 &&
      processor["terminatedThreadCount"].GetInt() == -1 &&
      processor["runStatus"].GetString() == std::string("RUNNING");
  }

  static bool verifyCommonRuntimeMetricNodes(const rapidjson::Value& runtime_metrics, const std::string& queue_id) {
    return runtime_metrics.HasMember("deviceInfo") &&
      runtime_metrics["deviceInfo"]["systemInfo"].HasMember("operatingSystem") &&
      runtime_metrics["deviceInfo"]["networkInfo"].HasMember("hostname") &&
      runtime_metrics.HasMember("flowInfo") &&
      runtime_metrics["flowInfo"].HasMember("flowId") &&
      runtime_metrics["flowInfo"].HasMember("runStatus") &&
      runtime_metrics["flowInfo"]["runStatus"].GetString() == std::string("RUNNING") &&
      runtime_metrics["flowInfo"].HasMember("versionedFlowSnapshotURI") &&
      runtime_metrics["flowInfo"].HasMember("queues") &&
      runtime_metrics["flowInfo"]["queues"].HasMember(queue_id) &&
      runtime_metrics.HasMember("agentInfo") &&
      runtime_metrics["agentInfo"]["status"]["repositories"]["ff"].HasMember("size") &&
      runtime_metrics["flowInfo"].HasMember("processorStatuses");
  }

  static bool verifyRuntimeMetrics(const rapidjson::Value& runtime_metrics) {
    return verifyCommonRuntimeMetricNodes(runtime_metrics, "2438e3c8-015a-1000-79ca-83af40ec1997") &&
      [&]() {
        const auto processor_statuses = runtime_metrics["flowInfo"]["processorStatuses"].GetArray();
        if (processor_statuses.Size() != 2) {
          return false;
        }
        return std::all_of(processor_statuses.begin(), processor_statuses.end(), [&](const auto& processor) {
          if (processor["id"].GetString() != std::string(GETTCP_UUID) && processor["id"].GetString() != std::string(LOGATTRIBUTE1_UUID)) {
            throw std::runtime_error(std::string("Unexpected processor id in processorStatuses: ") + processor["id"].GetString());
          }
          return processorMetricsAreValid(processor);
        });
      }();
  }

  static bool verifyUpdatedRuntimeMetrics(const rapidjson::Value& runtime_metrics) {
    return verifyCommonRuntimeMetricNodes(runtime_metrics, "8368e3c8-015a-1003-52ca-83af40ec1332") &&
      runtime_metrics["flowInfo"].HasMember("processorStatuses") &&
      [&]() {
        const auto processor_statuses = runtime_metrics["flowInfo"]["processorStatuses"].GetArray();
        if (processor_statuses.Size() != 2) {
          return false;
        }
        return std::all_of(processor_statuses.begin(), processor_statuses.end(), [&](const auto& processor) {
          if (processor["id"].GetString() != std::string(GENERATE_FLOWFILE_UUID) && processor["id"].GetString() != std::string(LOGATTRIBUTE2_UUID)) {
            throw std::runtime_error(std::string("Unexpected processor id in processorStatuses: ") + processor["id"].GetString());
          }
          return processorMetricsAreValid(processor);
        });
      }();
  }

  static bool verifyLoadMetrics(const rapidjson::Value& load_metrics) {
    return load_metrics.HasMember("RepositoryMetrics") &&
      load_metrics.HasMember("QueueMetrics") &&
      load_metrics["RepositoryMetrics"].HasMember("ff") &&
      load_metrics["RepositoryMetrics"].HasMember("repo_name") &&
      load_metrics["QueueMetrics"].HasMember("GetTCP/success/LogAttribute");
  }

  static bool verifyUpdatedLoadMetrics(const rapidjson::Value& load_metrics) {
    return load_metrics.HasMember("RepositoryMetrics") &&
      load_metrics.HasMember("QueueMetrics") &&
      load_metrics["RepositoryMetrics"].HasMember("ff") &&
      load_metrics["RepositoryMetrics"].HasMember("repo_name") &&
      load_metrics["QueueMetrics"].HasMember("GenerateFlowFile/success/LogAttribute") &&
      std::stoi(load_metrics["QueueMetrics"]["GenerateFlowFile/success/LogAttribute"]["queued"].GetString()) > 0;
  }

  static bool verifyProcessorMetrics(const rapidjson::Value& processor_metrics) {
    return processor_metrics.HasMember("GetTCPMetrics") &&
      processor_metrics["GetTCPMetrics"].HasMember(GETTCP_UUID) &&
      processor_metrics["GetTCPMetrics"][GETTCP_UUID].HasMember("OnTriggerInvocations") &&
      processor_metrics["GetTCPMetrics"][GETTCP_UUID]["OnTriggerInvocations"].GetUint() > 0 &&
      processor_metrics["GetTCPMetrics"][GETTCP_UUID].HasMember("TransferredFlowFiles") &&
      processor_metrics["GetTCPMetrics"][GETTCP_UUID].HasMember("AverageOnTriggerRunTime") &&
      processor_metrics["GetTCPMetrics"][GETTCP_UUID].HasMember("LastOnTriggerRunTime") &&
      processor_metrics["GetTCPMetrics"][GETTCP_UUID].HasMember("TransferredBytes") &&
      processor_metrics["GetTCPMetrics"][GETTCP_UUID].HasMember("IncomingFlowFiles") &&
      processor_metrics["GetTCPMetrics"][GETTCP_UUID].HasMember("IncomingBytes") &&
      processor_metrics["GetTCPMetrics"][GETTCP_UUID].HasMember("BytesRead") &&
      processor_metrics["GetTCPMetrics"][GETTCP_UUID].HasMember("BytesWritten") &&
      processor_metrics["GetTCPMetrics"][GETTCP_UUID].HasMember("ProcessingNanos");
  }

  std::atomic_bool& metrics_updated_successfully_;
  TestState test_state_ = TestState::VERIFY_INITIAL_METRICS;
  std::string replacement_config_;
};

TEST_CASE("C2MetricsTest", "[c2test]") {
  std::atomic_bool metrics_updated_successfully{false};
  VerifyC2Metrics harness(metrics_updated_successfully);
  harness.getConfiguration()->set("nifi.c2.root.classes", "FlowInformation,AgentInformation");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.name", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics", "runtimemetrics,loadmetrics,processorMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.runtimemetrics.name", "RuntimeMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.runtimemetrics.classes", "DeviceInfoNode,FlowInformation,AssetInformation,DeviceInfoNode,AgentInformation");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.loadmetrics.name", "LoadMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.loadmetrics.classes", "QueueMetrics,RepositoryMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processorMetrics.name", "ProcessorMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processorMetrics.classes", "processorMetrics/GetTCP.*");
  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestC2Metrics.yml";
  auto replacement_path = test_file_path.string();
  minifi::utils::string::replaceAll(replacement_path, "TestC2Metrics", "TestC2MetricsUpdate");
  MetricsHandler handler(metrics_updated_successfully, harness.getConfiguration(), replacement_path);
  harness.setUrl("https://localhost:0/api/heartbeat", &handler);
  harness.run(test_file_path);
}

}  // namespace org::apache::nifi::minifi::test
