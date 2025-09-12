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
  explicit VerifyEmptyC2Metric(const std::filesystem::path& test_file_path, const std::atomic_bool& metrics_found) : VerifyC2Base(test_file_path), metrics_found_(metrics_found) {
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
  static void sendEmptyHeartbeatResponse(struct mg_connection* conn) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
  }

  void verifyMetrics(const rapidjson::Document& root) {
    auto initial_metrics_verified =
      root.HasMember("metrics") &&
      root["metrics"].HasMember("LoadMetrics") &&
      verifyLoadMetrics(root["metrics"]["LoadMetrics"]);
    if (initial_metrics_verified) {
      metrics_found_ = true;
    }
  }

  static bool verifyLoadMetrics(const rapidjson::Value& load_metrics) {
    return load_metrics.HasMember("RepositoryMetrics") &&
      !load_metrics.HasMember("QueueMetrics") &&
      load_metrics["RepositoryMetrics"].HasMember("ff") &&
      load_metrics["RepositoryMetrics"].HasMember("repo_name");
  }

  std::atomic_bool& metrics_found_;
};

TEST_CASE("C2EmptyMetricTest", "[c2test]") {
  std::atomic_bool metrics_found{false};
  VerifyEmptyC2Metric harness(std::filesystem::path(TEST_RESOURCES) / "TestEmpty.yml", metrics_found);
  harness.getConfiguration()->set("nifi.c2.root.class.definitions", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.name", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics", "loadmetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.loadmetrics.name", "LoadMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.loadmetrics.classes", "QueueMetrics,RepositoryMetrics");
  MetricsHandler handler(metrics_found, harness.getConfiguration());
  harness.setUrl("http://localhost:0/api/heartbeat", &handler);
  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
