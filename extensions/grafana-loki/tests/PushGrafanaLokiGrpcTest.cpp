/**
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

#include "../PushGrafanaLokiGrpc.h"
#include "MockGrafanaLokiGrpc.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/Catch.h"
#include "utils/StringUtils.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki::test {

TEST_CASE("Url property is required", "[PushGrafanaLokiGrpc]") {
  minifi::test::SingleProcessorTestController test_controller(minifi::test::utils::make_processor<PushGrafanaLokiGrpc>("PushGrafanaLokiGrpc"));
  auto push_grafana_loki_grpc = test_controller.getProcessor();
  test_controller.plan->setProperty(push_grafana_loki_grpc, PushGrafanaLokiGrpc::Url, "");
  test_controller.plan->setProperty(push_grafana_loki_grpc, PushGrafanaLokiGrpc::StreamLabels, "job=minifi,directory=/opt/minifi/logs/");
  test_controller.plan->setProperty(push_grafana_loki_grpc, PushGrafanaLokiGrpc::LogLineBatchSize, "1");
  REQUIRE_THROWS_AS(test_controller.trigger(), minifi::Exception);
}

TEST_CASE("Valid stream labels need to be set", "[PushGrafanaLokiGrpc]") {
  minifi::test::SingleProcessorTestController test_controller(minifi::test::utils::make_processor<PushGrafanaLokiGrpc>("PushGrafanaLokiGrpc"));
  auto push_grafana_loki_grpc = test_controller.getProcessor();
  test_controller.plan->setProperty(push_grafana_loki_grpc, PushGrafanaLokiGrpc::Url, "localhost:10991");
  test_controller.plan->setProperty(push_grafana_loki_grpc, PushGrafanaLokiGrpc::LogLineBatchSize, "1");
  SECTION("Stream labels cannot be empty") {
    test_controller.plan->setProperty(push_grafana_loki_grpc, PushGrafanaLokiGrpc::StreamLabels, "");
  }
  SECTION("Stream labels need to be valid") {
    test_controller.plan->setProperty(push_grafana_loki_grpc, PushGrafanaLokiGrpc::StreamLabels, "invalidlabels,invalidlabels2");
  }
  REQUIRE_THROWS_AS(test_controller.trigger(), minifi::Exception);
}

TEST_CASE("Log Line Batch Size cannot be 0", "[PushGrafanaLokiGrpc]") {
  minifi::test::SingleProcessorTestController test_controller(minifi::test::utils::make_processor<PushGrafanaLokiGrpc>("PushGrafanaLokiGrpc"));
  auto push_grafana_loki_grpc = test_controller.getProcessor();
  CHECK(test_controller.plan->setProperty(push_grafana_loki_grpc, PushGrafanaLokiGrpc::Url, "localhost:10991"));
  CHECK(test_controller.plan->setProperty(push_grafana_loki_grpc, PushGrafanaLokiGrpc::StreamLabels, "job=minifi,directory=/opt/minifi/logs/"));
  test_controller.plan->setProperty(push_grafana_loki_grpc, PushGrafanaLokiGrpc::LogLineBatchSize, "0");
  REQUIRE_THROWS_AS(test_controller.trigger(), minifi::Exception);
}

class PushGrafanaLokiGrpcTestFixture {
 public:
  PushGrafanaLokiGrpcTestFixture()
      : mock_loki_("10991"),
        test_controller_(minifi::test::utils::make_processor<PushGrafanaLokiGrpc>("PushGrafanaLokiGrpc")),
        push_grafana_loki_grpc_(test_controller_.getProcessor()) {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setTrace<PushGrafanaLokiGrpc>();
    CHECK(test_controller_.plan->setProperty(push_grafana_loki_grpc_, PushGrafanaLokiGrpc::Url, "localhost:10991"));
    CHECK(test_controller_.plan->setProperty(push_grafana_loki_grpc_, PushGrafanaLokiGrpc::StreamLabels, "job=minifi,directory=/opt/minifi/logs/"));
  }

  void setProperty(const auto& property, const std::string& property_value) {
    CHECK(test_controller_.plan->setProperty(push_grafana_loki_grpc_, property, property_value));
  }

  void verifyLastRequestIsEmpty() {
    REQUIRE(mock_loki_.getLastRequest().entries.empty());
  }

  void verifyTenantId(const std::string& tenant_id) {
    REQUIRE(mock_loki_.getLastTenantId() == tenant_id);
  }

  void verifyStreamLabels() {
    const auto request = mock_loki_.getLastRequest();
    CHECK(request.stream_labels == "{directory=\"/opt/minifi/logs/\", job=\"minifi\"}");
  }

  void verifySentRequestToLoki(uint64_t start_timestamp, const std::vector<std::string>& expected_log_values,
      const std::vector<std::map<std::string, std::string>>& expected_log_line_attribute_values = {}) {
    const auto request = mock_loki_.getLastRequest();
    CHECK(request.entries.size() == expected_log_values.size());
    for (size_t i = 0; i < request.entries.size(); ++i) {
      CHECK(start_timestamp <= request.entries[i].timestamp);
      CHECK(request.entries[i].line == expected_log_values[i]);
      if (!expected_log_line_attribute_values.empty()) {
        REQUIRE(request.entries[i].labels.size() == expected_log_line_attribute_values[i].size());
        CHECK(request.entries[i].labels == expected_log_line_attribute_values[i]);
      }
    }
  }

  void verifyTransferredFlowContent(const std::vector<std::shared_ptr<core::FlowFile>>& flow_files, const std::vector<std::string>& expected_log_values) const {
    CHECK(flow_files.size() == expected_log_values.size());
    for (const auto& flow_file : flow_files) {
      CHECK(std::find(expected_log_values.begin(), expected_log_values.end(), test_controller_.plan->getContent(flow_file)) != expected_log_values.end());
    }
  }

 protected:
  MockGrafanaLokiGrpc mock_loki_;
  minifi::test::SingleProcessorTestController test_controller_;
  TypedProcessorWrapper<PushGrafanaLokiGrpc> push_grafana_loki_grpc_;
};

TEST_CASE_METHOD(PushGrafanaLokiGrpcTestFixture, "PushGrafanaLokiGrpc should send 1 log line to Grafana Loki in a trigger", "[PushGrafanaLokiGrpc]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  setProperty(PushGrafanaLokiGrpc::LogLineBatchSize, "1");
  setProperty(PushGrafanaLokiGrpc::MaxBatchSize, "1");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"孫子兵法", {}}, minifi::test::InputFlowFileData{"Война и мир", {}}});
  verifyStreamLabels();
  std::vector<std::string> expected_log_values = {"孫子兵法"};
  verifySentRequestToLoki(start_timestamp, expected_log_values);
  verifyTransferredFlowContent(results.at(PushGrafanaLokiGrpc::Success), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiGrpcTestFixture, "PushGrafanaLokiGrpc should wait for Log Line Batch Size limit to be reached", "[PushGrafanaLokiGrpc]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  setProperty(PushGrafanaLokiGrpc::LogLineBatchSize, "4");
  setProperty(PushGrafanaLokiGrpc::MaxBatchSize, "2");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}, minifi::test::InputFlowFileData{"log line 2", {}}, minifi::test::InputFlowFileData{"log line 3", {}}});
  CHECK(results.at(PushGrafanaLokiGrpc::Success).empty());
  verifyLastRequestIsEmpty();
  results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 4", {}}});
  verifyStreamLabels();
  std::vector<std::string> expected_log_values = {"log line 1", "log line 2", "log line 3", "log line 4"};
  verifySentRequestToLoki(start_timestamp, expected_log_values);
  verifyTransferredFlowContent(results.at(PushGrafanaLokiGrpc::Success), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiGrpcTestFixture, "Multiple batches are sent in a single trigger", "[PushGrafanaLokiGrpc]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  setProperty(PushGrafanaLokiGrpc::LogLineBatchSize, "2");
  setProperty(PushGrafanaLokiGrpc::MaxBatchSize, "4");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}, minifi::test::InputFlowFileData{"log line 2", {}}, minifi::test::InputFlowFileData{"log line 3", {}},
    minifi::test::InputFlowFileData{"log line 4", {}}, minifi::test::InputFlowFileData{"log line 5", {}}});
  verifyStreamLabels();
  std::vector<std::string> expected_log_values = {"log line 1", "log line 2", "log line 3", "log line 4"};
  verifySentRequestToLoki(start_timestamp, {"log line 3", "log line 4"});
  verifyTransferredFlowContent(results.at(PushGrafanaLokiGrpc::Success), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiGrpcTestFixture, "If submitting to Grafana Loki fails then the flow files should be transferred to failure", "[PushGrafanaLokiGrpc]") {
  setProperty(PushGrafanaLokiGrpc::Url, "http://invalid-url");
  setProperty(PushGrafanaLokiGrpc::LogLineBatchSize, "4");
  setProperty(PushGrafanaLokiGrpc::MaxBatchSize, "2");
  setProperty(PushGrafanaLokiGrpc::ConnectTimeout, "100 ms");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}, minifi::test::InputFlowFileData{"log line 2", {}}, minifi::test::InputFlowFileData{"log line 3", {}}});
  CHECK(results.at(PushGrafanaLokiGrpc::Success).empty());
  CHECK(results.at(PushGrafanaLokiGrpc::Failure).empty());
  results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 4", {}}});
  std::vector<std::string> expected_log_values = {"log line 1", "log line 2", "log line 3", "log line 4"};
  CHECK(results.at(PushGrafanaLokiGrpc::Success).empty());
  verifyTransferredFlowContent(results.at(PushGrafanaLokiGrpc::Failure), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiGrpcTestFixture, "If no log line batch limit is set, all log files in a single trigger should be processed", "[PushGrafanaLokiGrpc]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  std::vector<std::string> expected_log_values;
  SECTION("Max Batch Size is set") {
    setProperty(PushGrafanaLokiGrpc::MaxBatchSize, "2");
    expected_log_values = {"log line 1", "log line 2"};
  }
  SECTION("No Max Batch Size is set") {
    expected_log_values = {"log line 1", "log line 2", "log line 3"};
  }
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}, minifi::test::InputFlowFileData{"log line 2", {}}, minifi::test::InputFlowFileData{"log line 3", {}}});
  verifyStreamLabels();
  verifySentRequestToLoki(start_timestamp, expected_log_values);
  verifyTransferredFlowContent(results.at(PushGrafanaLokiGrpc::Success), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiGrpcTestFixture, "Log line metadata can be added with flow file attributes", "[PushGrafanaLokiGrpc]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  setProperty(PushGrafanaLokiGrpc::MaxBatchSize, "2");
  setProperty(PushGrafanaLokiGrpc::LogLineMetadataAttributes, "label1, label2, label3");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {{"label1", "value1"}, {"label4", "value4"}}},
    minifi::test::InputFlowFileData{"log line 2", {{"label1", "value1"}, {"label2", "value2"}}}, minifi::test::InputFlowFileData{"log line 3", {}}});
  verifyStreamLabels();
  std::vector<std::string> expected_log_values = {"log line 1", "log line 2"};
  std::vector<std::map<std::string, std::string>> expected_log_line_attribute_values = {{{"label1", "value1"}}, {{"label1", "value1"}, {"label2", "value2"}}};
  verifySentRequestToLoki(start_timestamp, expected_log_values, expected_log_line_attribute_values);
  verifyTransferredFlowContent(results.at(PushGrafanaLokiGrpc::Success), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiGrpcTestFixture, "Tenant ID can be set in properties", "[PushGrafanaLokiGrpc]") {
  setProperty(PushGrafanaLokiGrpc::LogLineBatchSize, "1");
  setProperty(PushGrafanaLokiGrpc::MaxBatchSize, "1");
  setProperty(PushGrafanaLokiGrpc::TenantID, "mytenant");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}});
  verifyTenantId("mytenant");
}

TEST_CASE_METHOD(PushGrafanaLokiGrpcTestFixture, "PushGrafanaLokiGrpc should wait for Log Line Batch Wait time to be reached", "[PushGrafanaLokiGrpc]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  setProperty(PushGrafanaLokiGrpc::LogLineBatchWait, "200 ms");
  setProperty(PushGrafanaLokiGrpc::MaxBatchSize, "3");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}, minifi::test::InputFlowFileData{"log line 2", {}}, minifi::test::InputFlowFileData{"log line 3", {}}});
  verifyLastRequestIsEmpty();
  std::this_thread::sleep_for(300ms);
  std::vector<std::string> expected_log_values;
  SECTION("Trigger with new flow file") {
    results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 4", {}}});
    expected_log_values = {"log line 1", "log line 2", "log line 3", "log line 4"};
  }
  SECTION("Trigger without new flow file should also send the batch") {
    results = test_controller_.trigger(std::vector<minifi::test::InputFlowFileData>{});
    expected_log_values = {"log line 1", "log line 2", "log line 3"};
  }
  verifyStreamLabels();
  verifySentRequestToLoki(start_timestamp, expected_log_values);
  verifyTransferredFlowContent(results.at(PushGrafanaLokiGrpc::Success), expected_log_values);
}

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki::test
