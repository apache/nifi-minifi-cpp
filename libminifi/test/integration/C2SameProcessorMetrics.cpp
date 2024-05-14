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

class VerifyEmptyC2Metric : public VerifyC2Base {
 public:
  explicit VerifyEmptyC2Metric(const std::atomic_bool& metrics_found) : metrics_found_(metrics_found) {
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
    REQUIRE(verifyEventHappenedInPollTime(40s, [&] { return metrics_found_.load(); }, 1s));
  }

 private:
  const std::atomic_bool& metrics_found_;
};

class MetricsHandler: public HeartbeatHandler {
 public:
  explicit MetricsHandler(std::atomic_bool& metrics_found, std::shared_ptr<minifi::Configure> configuration)
    : HeartbeatHandler(std::move(configuration)),
      metrics_found_(metrics_found) {
  }

  void handleHeartbeat(const rapidjson::Document& root, struct mg_connection* conn) override {
    verifyMetrics(root);
    sendEmptyHeartbeatResponse(conn);
  }

 private:
  static constexpr const char* GETFILE1_UUID = "471deef6-2a6e-4a7d-912a-81cc17e3a206";
  static constexpr const char* GETFILE2_UUID = "471deef6-2a6e-4a7d-912a-81cc17e3a207";
  static constexpr const char* GETTCP1_UUID = "2438e3c8-015a-1000-79ca-83af40ec1995";
  static constexpr const char* GETTCP2_UUID = "2438e3c8-015a-1000-79ca-83af40ec1996";

  static void sendEmptyHeartbeatResponse(struct mg_connection* conn) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
  }

  void verifyMetrics(const rapidjson::Document& root) {
    auto initial_metrics_verified =
      root.HasMember("metrics") &&
      root["metrics"].HasMember("ProcessorMetrics") &&
      root["metrics"]["ProcessorMetrics"].HasMember("GetFileMetrics") &&
      root["metrics"]["ProcessorMetrics"]["GetFileMetrics"].HasMember(GETFILE1_UUID) &&
      root["metrics"]["ProcessorMetrics"]["GetFileMetrics"].HasMember(GETFILE2_UUID) &&
      root["metrics"]["ProcessorMetrics"]["GetTCPMetrics"].HasMember(GETTCP1_UUID) &&
      root["metrics"]["ProcessorMetrics"]["GetTCPMetrics"].HasMember(GETTCP2_UUID);
    if (initial_metrics_verified) {
      metrics_found_ = true;
    }
  }

  std::atomic_bool& metrics_found_;
};

TEST_CASE("Test support for setting metrics for multiple processors of the same type in a flow", "[c2test]") {
  std::atomic_bool metrics_found{false};
  VerifyEmptyC2Metric harness(metrics_found);
  harness.getConfiguration()->set("nifi.c2.root.class.definitions", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.name", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics", "processormetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processormetrics.name", "ProcessorMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processormetrics.classes", "GetFileMetrics,GetTCPMetrics");
  MetricsHandler handler(metrics_found, harness.getConfiguration());
  harness.setUrl("http://localhost:0/api/heartbeat", &handler);
  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestSameProcessorMetrics.yml";
  harness.run(test_file_path);
}

}  // namespace org::apache::nifi::minifi::test
