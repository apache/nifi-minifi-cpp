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
#include "processors/TailFile.h"
#include "state/ProcessorController.h"
#include "utils/file/FileUtils.h"
#include "utils/TestUtils.h"
#include "processors/GetTCP.h"
#include "utils/StringUtils.h"
#include "utils/file/PathUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class VerifyEmptyC2Metric : public VerifyC2Base {
 public:
  explicit VerifyEmptyC2Metric(const std::atomic_bool& metrics_found) : metrics_found_(metrics_found) {
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<c2::C2Agent>();
    LogTestController::getInstance().setDebug<c2::RESTSender>();
    LogTestController::getInstance().setDebug<FlowController>();
    LogTestController::getInstance().setOff<processors::GetTCP>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(40s, [&] { return metrics_found_.load(); }, 1s));
  }

 private:
  const std::atomic_bool& metrics_found_;
};

class MetricsHandler: public HeartbeatHandler {
 public:
  explicit MetricsHandler(std::atomic_bool& metrics_found, std::shared_ptr<Configure> configuration)
    : HeartbeatHandler(std::move(configuration)),
      metrics_found_(metrics_found) {
  }

  void handleHeartbeat(const rapidjson::Document&, struct mg_connection* conn) override {
    switch (state_) {
      case TestState::DESCRIBE_SPECIFIC_PROCESSOR_METRIC: {
        sendHeartbeatResponse("DESCRIBE", "metrics", "889347", conn, {{"metricsClass", "GetFileMetrics"}});
        break;
      }
      case TestState::DESCRIBE_SPECIFIC_SYSTEM_METRIC: {
        sendHeartbeatResponse("DESCRIBE", "metrics", "889347", conn, {{"metricsClass", "QueueMetrics"}});
        break;
      }
      case TestState::DESCRIBE_ALL_METRICS: {
        sendHeartbeatResponse("DESCRIBE", "metrics", "889347", conn);
        break;
      }
      default:
        throw std::runtime_error("Unhandled test state");
    }
  }

  void handleAcknowledge(const rapidjson::Document& root) override {
    switch (state_) {
      case TestState::DESCRIBE_SPECIFIC_PROCESSOR_METRIC: {
        verifySpecificProcessorMetrics(root);
        break;
      }
      case TestState::DESCRIBE_SPECIFIC_SYSTEM_METRIC: {
        verifySpecificSystemMetrics(root);
        break;
      }
      case TestState::DESCRIBE_ALL_METRICS: {
        verifyAllMetrics(root);
        break;
      }
      default:
        throw std::runtime_error("Unhandled test state");
    }
  }

 private:
  enum class TestState {
    DESCRIBE_SPECIFIC_PROCESSOR_METRIC,
    DESCRIBE_SPECIFIC_SYSTEM_METRIC,
    DESCRIBE_ALL_METRICS
  };

  static constexpr const char* GETFILE1_UUID = "471deef6-2a6e-4a7d-912a-81cc17e3a206";
  static constexpr const char* GETFILE2_UUID = "471deef6-2a6e-4a7d-912a-81cc17e3a207";
  static constexpr const char* GETTCP1_UUID = "2438e3c8-015a-1000-79ca-83af40ec1995";
  static constexpr const char* GETTCP2_UUID = "2438e3c8-015a-1000-79ca-83af40ec1996";

  static void sendEmptyHeartbeatResponse(struct mg_connection* conn) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
  }

  void verifySpecificProcessorMetrics(const rapidjson::Document& root) {
    auto getfile_metrics_verified =
      !root.HasMember("metrics") &&
      root.HasMember("GetFileMetrics") &&
      root["GetFileMetrics"].HasMember(GETFILE1_UUID) &&
      root["GetFileMetrics"].HasMember(GETFILE2_UUID);
    if (getfile_metrics_verified) {
      state_ = TestState::DESCRIBE_SPECIFIC_SYSTEM_METRIC;
    }
  }

  void verifySpecificSystemMetrics(const rapidjson::Document& root) {
    auto getfile_metrics_verified =
      !root.HasMember("metrics") &&
      root.HasMember("QueueMetrics");
    if (getfile_metrics_verified) {
      state_ = TestState::DESCRIBE_ALL_METRICS;
    }
  }

  void verifyAllMetrics(const rapidjson::Document& root) {
    auto all_metrics_verified =
      root.HasMember("metrics") &&
      root["metrics"].HasMember("ProcessorMetrics") &&
      root["metrics"]["ProcessorMetrics"].HasMember("GetFileMetrics") &&
      root["metrics"]["ProcessorMetrics"]["GetFileMetrics"].HasMember(GETFILE1_UUID) &&
      root["metrics"]["ProcessorMetrics"]["GetFileMetrics"].HasMember(GETFILE2_UUID) &&
      root["metrics"]["ProcessorMetrics"]["GetTCPMetrics"].HasMember(GETTCP1_UUID) &&
      root["metrics"]["ProcessorMetrics"]["GetTCPMetrics"].HasMember(GETTCP2_UUID) &&
      root["metrics"].HasMember("SystemMetrics") &&
      root["metrics"]["SystemMetrics"].HasMember("QueueMetrics");
    if (all_metrics_verified) {
      metrics_found_ = true;
    }
  }

  TestState state_ = TestState::DESCRIBE_SPECIFIC_PROCESSOR_METRIC;
  std::atomic_bool& metrics_found_;
};

}  // namespace org::apache::nifi::minifi::test

int main(int argc, char **argv) {
  std::atomic_bool metrics_found{false};
  const cmd_args args = parse_cmdline_args(argc, argv, "api/heartbeat");
  org::apache::nifi::minifi::test::VerifyEmptyC2Metric harness(metrics_found);
  harness.getConfiguration()->set("nifi.c2.root.class.definitions", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.name", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics", "processormetrics,systemmetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processormetrics.name", "ProcessorMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processormetrics.classes", "GetFileMetrics,GetTCPMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.systemmetrics.name", "SystemMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.systemmetrics.classes", "QueueMetrics");
  harness.setKeyDir(args.key_dir);
  org::apache::nifi::minifi::test::MetricsHandler handler(metrics_found, harness.getConfiguration());
  harness.setUrl(args.url, &handler);
  harness.run(args.test_file);
  return 0;
}
