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

#include "../PushGrafanaLokiREST.h"
#include "MockGrafanaLoki.h"
#include "SingleProcessorTestController.h"
#include "Catch.h"
#include "utils/StringUtils.h"
#include "utils/TestUtils.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki::test {

TEST_CASE("Url property is required", "[PushGrafanaLokiREST]") {
  auto push_grafana_loki_rest = std::make_shared<PushGrafanaLokiREST>("PushGrafanaLokiREST");
  minifi::test::SingleProcessorTestController test_controller(push_grafana_loki_rest);
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::Url, ""));
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::StreamLabels, "job=minifi,directory=/opt/minifi/logs/"));
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::LogLineBatchSize, "1"));
  REQUIRE_THROWS_AS(test_controller.trigger(), minifi::Exception);
}

TEST_CASE("Valid stream labels need to be set", "[PushGrafanaLokiREST]") {
  auto push_grafana_loki_rest = std::make_shared<PushGrafanaLokiREST>("PushGrafanaLokiREST");
  minifi::test::SingleProcessorTestController test_controller(push_grafana_loki_rest);
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::Url, "localhost:10990"));
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::LogLineBatchSize, "1"));
  SECTION("Stream labels cannot be empty") {
    test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::StreamLabels, "");
  }
  SECTION("Stream labels need to be valid") {
    test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::StreamLabels, "invalidlabels,invalidlabels2");
  }
  REQUIRE_THROWS_AS(test_controller.trigger(), minifi::Exception);
}

TEST_CASE("Log Line Batch Size cannot be 0", "[PushGrafanaLokiREST]") {
  auto push_grafana_loki_rest = std::make_shared<PushGrafanaLokiREST>("PushGrafanaLokiREST");
  minifi::test::SingleProcessorTestController test_controller(push_grafana_loki_rest);
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::Url, "localhost:10990"));
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::StreamLabels, "job=minifi,directory=/opt/minifi/logs/"));
  test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::LogLineBatchSize, "0");
  REQUIRE_THROWS_AS(test_controller.trigger(), minifi::Exception);
}

class PushGrafanaLokiRESTTestFixture {
 public:
  PushGrafanaLokiRESTTestFixture()
      : mock_loki_("10990"),
        push_grafana_loki_rest_(std::make_shared<PushGrafanaLokiREST>("PushGrafanaLokiREST")),
        test_controller_(push_grafana_loki_rest_) {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setTrace<PushGrafanaLokiREST>();
    CHECK(test_controller_.plan->setProperty(push_grafana_loki_rest_, PushGrafanaLokiREST::Url, "localhost:10990"));
    CHECK(test_controller_.plan->setProperty(push_grafana_loki_rest_, PushGrafanaLokiREST::StreamLabels, "job=minifi,directory=/opt/minifi/logs/"));
  }

  void setProperty(const auto& property, const std::string& property_value) {
    CHECK(test_controller_.plan->setProperty(push_grafana_loki_rest_, property, property_value));
  }

  void verifyLastRequestIsEmpty() {
    const auto& request = mock_loki_.getLastRequest();
    REQUIRE(request.IsNull());
  }

  void verifyTenantId(const std::string& tenant_id) {
    REQUIRE(mock_loki_.getLastTenantId() == tenant_id);
  }

  void verifyBasicAuthorization(const std::string& expected_username_and_password) {
    auto last_authorization = mock_loki_.getLastAuthorization();
    std::string expected_authorization = "Basic ";
    REQUIRE(minifi::utils::StringUtils::startsWith(last_authorization, expected_authorization));
    std::string username_and_password_decoded = minifi::utils::StringUtils::from_base64(last_authorization.substr(expected_authorization.size()), minifi::utils::as_string_tag_t{});
    REQUIRE(username_and_password_decoded == expected_username_and_password);
  }

  void verifyBearerTokenAuthorization(const std::string& expected_bearer_token) {
    auto last_authorization = mock_loki_.getLastAuthorization();
    std::string expected_authorization = "Bearer ";
    REQUIRE(minifi::utils::StringUtils::startsWith(last_authorization, expected_authorization));
    auto bearer_token = last_authorization.substr(expected_authorization.size());
    REQUIRE(bearer_token == expected_bearer_token);
  }

  void verifyStreamLabels() {
    const auto& request = mock_loki_.getLastRequest();
    REQUIRE(request.HasMember("streams"));
    const auto& stream_array = request["streams"].GetArray();
    REQUIRE(stream_array.Size() == 1);
    REQUIRE(stream_array[0].HasMember("stream"));
    const auto& stream = stream_array[0]["stream"].GetObject();
    REQUIRE(stream.HasMember("job"));
    std::string job_string = stream["job"].GetString();
    REQUIRE(job_string == "minifi");
    REQUIRE(stream.HasMember("directory"));
    std::string directory_string = stream["directory"].GetString();
    REQUIRE(directory_string == "/opt/minifi/logs/");
  }

  void verifySentRequestToLoki(uint64_t start_timestamp, const std::vector<std::string>& expected_log_values,
      const std::vector<std::map<std::string, std::string>>& expected_log_line_attribute_values = {}) {
    const auto& request = mock_loki_.getLastRequest();
    REQUIRE(request.HasMember("streams"));
    const auto& stream_array = request["streams"].GetArray();
    REQUIRE(stream_array[0].HasMember("values"));
    const auto& value_array = stream_array[0]["values"].GetArray();
    REQUIRE(value_array.Size() == expected_log_values.size());
    for (size_t i = 0; i < expected_log_values.size(); ++i) {
      const auto& log_line_array = value_array[i].GetArray();
      if (!expected_log_line_attribute_values.empty()) {
        REQUIRE(log_line_array.Size() == 3);
      } else {
        REQUIRE(log_line_array.Size() == 2);
      }
      std::string timestamp_str = log_line_array[0].GetString();
      REQUIRE(start_timestamp <= std::stoull(timestamp_str));
      std::string value = log_line_array[1].GetString();
      REQUIRE(value == expected_log_values[i]);
      if (!expected_log_line_attribute_values.empty()) {
        REQUIRE(log_line_array[2].IsObject());
        const auto& log_line_attribute_object = log_line_array[2].GetObject();
        for (const auto& [key, value] : expected_log_line_attribute_values[i]) {
          REQUIRE(log_line_attribute_object.HasMember(key.c_str()));
          REQUIRE(log_line_attribute_object[key.c_str()].GetString() == value);
        }
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
  MockGrafanaLoki mock_loki_;
  std::shared_ptr<PushGrafanaLokiREST> push_grafana_loki_rest_;
  minifi::test::SingleProcessorTestController test_controller_;
};

TEST_CASE_METHOD(PushGrafanaLokiRESTTestFixture, "PushGrafanaLokiREST should send 1 log line to Grafana Loki in a trigger", "[PushGrafanaLokiREST]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  setProperty(PushGrafanaLokiREST::LogLineBatchSize, "1");
  setProperty(PushGrafanaLokiREST::MaxBatchSize, "1");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}, minifi::test::InputFlowFileData{"log line 2", {}}});
  verifyStreamLabels();
  std::vector<std::string> expected_log_values = {"log line 1"};
  verifySentRequestToLoki(start_timestamp, expected_log_values);
  verifyTransferredFlowContent(results.at(PushGrafanaLokiREST::Success), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiRESTTestFixture, "PushGrafanaLokiREST should wait for Log Line Batch Size limit to be reached", "[PushGrafanaLokiREST]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  setProperty(PushGrafanaLokiREST::LogLineBatchSize, "4");
  setProperty(PushGrafanaLokiREST::MaxBatchSize, "2");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}, minifi::test::InputFlowFileData{"log line 2", {}}, minifi::test::InputFlowFileData{"log line 3", {}}});
  CHECK(results.at(PushGrafanaLokiREST::Success).empty());
  verifyLastRequestIsEmpty();
  results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 4", {}}});
  verifyStreamLabels();
  std::vector<std::string> expected_log_values = {"log line 1", "log line 2", "log line 3", "log line 4"};
  verifySentRequestToLoki(start_timestamp, expected_log_values);
  verifyTransferredFlowContent(results.at(PushGrafanaLokiREST::Success), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiRESTTestFixture, "Multiple batches are sent in a single trigger", "[PushGrafanaLokiREST]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  setProperty(PushGrafanaLokiREST::LogLineBatchSize, "2");
  setProperty(PushGrafanaLokiREST::MaxBatchSize, "4");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}, minifi::test::InputFlowFileData{"log line 2", {}}, minifi::test::InputFlowFileData{"log line 3", {}},
    minifi::test::InputFlowFileData{"log line 4", {}}, minifi::test::InputFlowFileData{"log line 5", {}}});
  verifyStreamLabels();
  std::vector<std::string> expected_log_values = {"log line 1", "log line 2", "log line 3", "log line 4"};
  verifySentRequestToLoki(start_timestamp, {"log line 3", "log line 4"});
  verifyTransferredFlowContent(results.at(PushGrafanaLokiREST::Success), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiRESTTestFixture, "If submitting to Grafana Loki fails then the flow files should be transferred to failure", "[PushGrafanaLokiREST]") {
  setProperty(PushGrafanaLokiREST::Url, "http://invalid-url");
  setProperty(PushGrafanaLokiREST::LogLineBatchSize, "4");
  setProperty(PushGrafanaLokiREST::MaxBatchSize, "2");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}, minifi::test::InputFlowFileData{"log line 2", {}}, minifi::test::InputFlowFileData{"log line 3", {}}});
  CHECK(results.at(PushGrafanaLokiREST::Success).empty());
  CHECK(results.at(PushGrafanaLokiREST::Failure).empty());
  results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 4", {}}});
  std::vector<std::string> expected_log_values = {"log line 1", "log line 2", "log line 3", "log line 4"};
  CHECK(results.at(PushGrafanaLokiREST::Success).empty());
  verifyTransferredFlowContent(results.at(PushGrafanaLokiREST::Failure), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiRESTTestFixture, "If no log line batch limit is set, all log files in a single trigger should be processed", "[PushGrafanaLokiREST]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  std::vector<std::string> expected_log_values;
  SECTION("Max Batch Size is set") {
    setProperty(PushGrafanaLokiREST::MaxBatchSize, "2");
    expected_log_values = {"log line 1", "log line 2"};
  }
  SECTION("No Max Batch Size is set") {
    expected_log_values = {"log line 1", "log line 2", "log line 3"};
  }
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}, minifi::test::InputFlowFileData{"log line 2", {}}, minifi::test::InputFlowFileData{"log line 3", {}}});
  verifyStreamLabels();
  verifySentRequestToLoki(start_timestamp, expected_log_values);
  verifyTransferredFlowContent(results.at(PushGrafanaLokiREST::Success), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiRESTTestFixture, "Log line metadata can be added with flow file attributes", "[PushGrafanaLokiREST]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  setProperty(PushGrafanaLokiREST::MaxBatchSize, "2");
  setProperty(PushGrafanaLokiREST::LogLineMetadataAttributes, "label1, label2, label3");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {{"label1", "value1"}, {"label4", "value4"}}},
    minifi::test::InputFlowFileData{"log line 2", {{"label1", "value1"}, {"label2", "value2"}}}, minifi::test::InputFlowFileData{"log line 3", {}}});
  verifyStreamLabels();
  std::vector<std::string> expected_log_values = {"log line 1", "log line 2"};
  std::vector<std::map<std::string, std::string>> expected_log_line_attribute_values = {{{"label1", "value1"}}, {{"label1", "value1"}, {"label2", "value2"}}};
  verifySentRequestToLoki(start_timestamp, expected_log_values, expected_log_line_attribute_values);
  verifyTransferredFlowContent(results.at(PushGrafanaLokiREST::Success), expected_log_values);
}

TEST_CASE_METHOD(PushGrafanaLokiRESTTestFixture, "Tenant ID can be set in properties", "[PushGrafanaLokiREST]") {
  setProperty(PushGrafanaLokiREST::LogLineBatchSize, "1");
  setProperty(PushGrafanaLokiREST::MaxBatchSize, "1");
  setProperty(PushGrafanaLokiREST::TenantID, "mytenant");
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}});
  verifyTenantId("mytenant");
}

TEST_CASE_METHOD(PushGrafanaLokiRESTTestFixture, "PushGrafanaLokiREST should wait for Log Line Batch Wait time to be reached", "[PushGrafanaLokiREST]") {
  uint64_t start_timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
  setProperty(PushGrafanaLokiREST::LogLineBatchWait, "200 ms");
  setProperty(PushGrafanaLokiREST::MaxBatchSize, "3");
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
  verifyTransferredFlowContent(results.at(PushGrafanaLokiREST::Success), expected_log_values);
}

TEST_CASE("If username is set, password is also required to be set", "[PushGrafanaLokiREST]") {
  auto push_grafana_loki_rest = std::make_shared<PushGrafanaLokiREST>("PushGrafanaLokiREST");
  minifi::test::SingleProcessorTestController test_controller(push_grafana_loki_rest);
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::Url, "localhost:10990"));
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::StreamLabels, "job=minifi,directory=/opt/minifi/logs/"));
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::LogLineBatchSize, "1"));
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::Username, "admin"));
  REQUIRE_THROWS_AS(test_controller.trigger(), minifi::Exception);
}

TEST_CASE_METHOD(PushGrafanaLokiRESTTestFixture, "Basic authentication is set in HTTP header", "[PushGrafanaLokiREST]") {
  setProperty(PushGrafanaLokiREST::LogLineBatchSize, "1");
  setProperty(PushGrafanaLokiREST::Username, "admin");
  setProperty(PushGrafanaLokiREST::Password, "admin");
  setProperty(PushGrafanaLokiREST::BearerTokenFile, "mytoken");  // Basic authentication should take precedence
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}});
  verifyBasicAuthorization("admin:admin");
}

TEST_CASE_METHOD(PushGrafanaLokiRESTTestFixture, "Bearer token is set for authentication", "[PushGrafanaLokiREST]") {
  auto temp_dir = test_controller_.createTempDirectory();
  auto test_file_path = minifi::utils::putFileToDir(temp_dir, "test1.txt", "mytoken\n");
  setProperty(PushGrafanaLokiREST::LogLineBatchSize, "1");
  setProperty(PushGrafanaLokiREST::BearerTokenFile, test_file_path.string());
  auto results = test_controller_.trigger({minifi::test::InputFlowFileData{"log line 1", {}}});
  verifyBearerTokenAuthorization("mytoken");
}

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki::test
