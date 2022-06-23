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
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class VerifyEmptyC2Metric : public VerifyC2Base {
 public:
  explicit VerifyEmptyC2Metric(const std::atomic_bool& metrics_found) : metrics_found_(metrics_found) {
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setTrace<minifi::c2::C2Client>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
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
  explicit MetricsHandler(std::atomic_bool& metrics_found, std::shared_ptr<minifi::Configure> configuration)
    : HeartbeatHandler(std::move(configuration)),
      metrics_found_(metrics_found) {
  }

  void handleHeartbeat(const rapidjson::Document&, struct mg_connection* conn) override {
    switch (state_) {
      case TestState::DESCRIBE_SPECIFIC_METRIC: {
        sendHeartbeatResponse("DESCRIBE", "metrics", "889347", conn, {{"metricsClass", "GetFileMetrics"}});
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
      case TestState::DESCRIBE_SPECIFIC_METRIC: {
        verifySpecificMetrics(root);
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
    DESCRIBE_SPECIFIC_METRIC,
    DESCRIBE_ALL_METRICS
  };

  static void sendEmptyHeartbeatResponse(struct mg_connection* conn) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
  }

  void verifySpecificMetrics(const rapidjson::Document& root) {
    rapidjson::StringBuffer buffer;
    buffer.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    root.Accept(writer);
    auto str = strdup(buffer.GetString());
    (void)str;
    auto getfile_metrics_verified =
      !root.HasMember("metrics") &&
      root.HasMember("GetFileMetrics") &&
      verifyGetFileMetrics(root["GetFileMetrics"]);
    if (getfile_metrics_verified) {
      state_ = TestState::DESCRIBE_ALL_METRICS;
    }
  }

  static bool verifyGetFileMetrics(const rapidjson::Value& getfile_metrics) {
    std::unordered_set<std::string> expected_names{"GetFile1", "GetFile2"};
    std::unordered_set<std::string> names;
    for (auto &get_file_metric : getfile_metrics.GetArray()) {
      for (auto member_it = get_file_metric.MemberBegin(); member_it != get_file_metric.MemberEnd(); ++member_it) {
        names.insert(member_it->name.GetString());
      }
    }

    return names == expected_names;
  }

  void verifyAllMetrics(const rapidjson::Document& root) {
    rapidjson::StringBuffer buffer;
    buffer.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    root.Accept(writer);
    auto str = strdup(buffer.GetString());
    (void)str;
    auto all_metrics_verified =
      root.HasMember("metrics") &&
      root["metrics"].HasMember("ProcessorMetrics") &&
      root["metrics"]["ProcessorMetrics"].HasMember("GetFileMetrics") &&
      verifyGetFileMetrics(root["metrics"]["ProcessorMetrics"]["GetFileMetrics"]) &&
      root["metrics"].HasMember("SystemMetrics") &&
      root["metrics"]["SystemMetrics"].HasMember("QueueMetrics");
    if (all_metrics_verified) {
      metrics_found_ = true;
    }
  }

  TestState state_ = TestState::DESCRIBE_SPECIFIC_METRIC;
  std::atomic_bool& metrics_found_;
};

}  // namespace org::apache::nifi::minifi::test

int main(int argc, char **argv) {
  std::atomic_bool metrics_found{false};
  const cmd_args args = parse_cmdline_args(argc, argv, "api/heartbeat");
  org::apache::nifi::minifi::test::VerifyEmptyC2Metric harness(metrics_found);
  harness.getConfiguration()->set("nifi.c2.root.class.definitions", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.name", "metrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics", "processormetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processormetrics.name", "ProcessorMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processormetrics.classes", "GetFileMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processormetrics.name", "SystemMetrics");
  harness.getConfiguration()->set("nifi.c2.root.class.definitions.metrics.metrics.processormetrics.classes", "QueueMetrics");
  harness.setKeyDir(args.key_dir);
  org::apache::nifi::minifi::test::MetricsHandler handler(metrics_found, harness.getConfiguration());
  harness.setUrl(args.url, &handler);
  harness.run(args.test_file);
  return 0;
}
