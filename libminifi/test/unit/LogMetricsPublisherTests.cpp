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
#include <thread>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/state/LogMetricsPublisher.h"
#include "core/state/nodes/ResponseNodeLoader.h"
#include "core/RepositoryFactory.h"
#include "unit/TestUtils.h"
#include "utils/file/FileUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class LogPublisherTestFixture {
 public:
  LogPublisherTestFixture()
    : configuration_(std::make_shared<ConfigureImpl>()),
      provenance_repo_(core::createRepository("provenancerepository", "provenancerepository")),
      flow_file_repo_(core::createRepository("flowfilerepository", "flowfilerepository")),
      response_node_loader_(std::make_shared<state::response::ResponseNodeLoaderImpl>(configuration_,
        std::vector<std::shared_ptr<core::RepositoryMetricsSource>>{provenance_repo_, flow_file_repo_}, nullptr)),
      publisher_(std::make_unique<minifi::state::LogMetricsPublisher>("LogMetricsPublisher")) {
    provenance_repo_->initialize(configuration_);
    flow_file_repo_->initialize(configuration_);
  }

  LogPublisherTestFixture(LogPublisherTestFixture&&) = delete;
  LogPublisherTestFixture(const LogPublisherTestFixture&) = delete;
  LogPublisherTestFixture& operator=(LogPublisherTestFixture&&) = delete;
  LogPublisherTestFixture& operator=(const LogPublisherTestFixture&) = delete;

  ~LogPublisherTestFixture() {
    publisher_.reset();  // explicit because LogTestController should outlive the thread in publisher_
    minifi::utils::file::delete_dir(provenance_repo_->getDirectory());
    minifi::utils::file::delete_dir(flow_file_repo_->getDirectory());
    LogTestController::getInstance().reset();
  }

 protected:
  TempDirectory temp_directory_;
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<core::Repository> provenance_repo_;
  std::shared_ptr<core::Repository> flow_file_repo_;
  std::shared_ptr<state::response::ResponseNodeLoader> response_node_loader_;
  std::unique_ptr<minifi::state::LogMetricsPublisher> publisher_;
};

TEST_CASE_METHOD(LogPublisherTestFixture, "Logging interval property is mandatory", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  SECTION("No logging interval is set") {
    REQUIRE_THROWS_WITH(publisher_->initialize(configuration_, response_node_loader_), "General Operation: Metrics logging interval not configured for log metrics publisher!");
  }
  SECTION("Logging interval is set to 2 seconds") {
    configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "2s");
    publisher_->initialize(configuration_, response_node_loader_);
    REQUIRE(utils::verifyLogLinePresenceInPollTime(5s, "Metric logging interval is set to 2000ms"));
  }
}

TEST_CASE_METHOD(LogPublisherTestFixture, "Verify empty metrics if no valid metrics are defined", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "100ms");
  SECTION("No metrics are defined") {}
  SECTION("Only invalid metrics are defined") {
    configuration_->set(Configure::nifi_metrics_publisher_metrics, "InvalidMetric,NotValidMetricNode");
  }
  publisher_->initialize(configuration_, response_node_loader_);
  publisher_->loadMetricNodes();
  REQUIRE(utils::verifyLogLinePresenceInPollTime(5s, "LogMetricsPublisher is configured without any valid metrics!"));
}

std::string expectedRepositoryMetricsRegex(std::string level) {
  std::string regex_pattern =
      // 1. Log Header & Opening
      R"(\[[\d\-\s\:\.]+\]\s*\[[^\]]+\]\s*\[)" + std::move(level) + R"(\]\s*\{\s*)" +
      R"(\"LogMetrics\":\s*\{\s*)" +

      // 2. (skips other nodes, stops if a \n[ starts (other log line))
      R"((?:(?!\n\[)[\s\S])*?)" +

      // 3. RepositoryMetrics Node
      R"(\"RepositoryMetrics\":\s*\{\s*)" +

      // 4. Provenance Repository
      R"(\"provenancerepository\":\s*\{\s*)" +
      R"(\"running\":\s*\"false\",\s*)" +
      R"(\"full\":\s*\"false\",\s*)" +
      R"(\"size\":\s*\"0\",\s*)" +
      R"(\s*\"maxSize\":\s*\"0\",\s*)" +
      R"(\"entryCount\":\s*\"0\",\s*)" +
      R"(\"rocksDbTableReadersSize\":\s*\"0\",\s*)" +
      R"(\"rocksDbAllMemoryTablesSize\":\s*\"2048\",\s*)" +
      R"(\"rocksDbBlockCacheUsage\":\s*\"\d+\",\s*)" +
      R"(\"rocksDbBlockCachePinnedUsage\":\s*\"\d+\"\s*)" +
      R"(\},\s*)" +

      // 5. FlowFile Repository
      R"(\"flowfilerepository\":\s*\{\s*)" +
      R"(\"running\":\s*\"false\",\s*)" +
      R"(\"full\":\s*\"false\",\s*)" +
      R"(\"size\":\s*\"0\",\s*)" +
      R"(\"maxSize\":\s*\"0\",\s*)" +
      R"(\"entryCount\":\s*\"0\",\s*)" +
      R"(\"rocksDbTableReadersSize\":\s*\"0\",\s*)" +
      R"(\"rocksDbAllMemoryTablesSize\":\s*\"2048\",\s*)" +
      R"(\"rocksDbBlockCacheUsage\":\s*\"\d+\",\s*)" +
      R"(\"rocksDbBlockCachePinnedUsage\":\s*\"\d+\"\s*)" +

      // 6. Final Closures
      R"(\}\s*\}\s*\}\s*\})";
  return regex_pattern;
}

TEST_CASE_METHOD(LogPublisherTestFixture, "Verify multiple metric nodes in logs", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "100ms");
  configuration_->set(Configure::nifi_metrics_publisher_metrics, "RepositoryMetrics,DeviceInfoNode");
  publisher_->initialize(configuration_, response_node_loader_);
  publisher_->loadMetricNodes();
  std::string expected_log_1 = R"([info] {
    "LogMetrics": {)";
  std::string expected_log_3 = R"("deviceInfo": {
            "identifier":)";
  REQUIRE(utils::verifyLogLinePresenceInPollTime(5s, expected_log_1));
  REQUIRE(utils::verifyLogMatchesRegexInPollTime(5s, expectedRepositoryMetricsRegex("info")));
  REQUIRE(utils::verifyLogLinePresenceInPollTime(5s, expected_log_3));
}

TEST_CASE_METHOD(LogPublisherTestFixture, "Verify reloading different metrics", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "100ms");
  configuration_->set(Configure::nifi_metrics_publisher_metrics, "RepositoryMetrics");
  publisher_->initialize(configuration_, response_node_loader_);
  publisher_->loadMetricNodes();

  REQUIRE(utils::verifyLogMatchesRegexInPollTime(5s, expectedRepositoryMetricsRegex("info")));
  publisher_->clearMetricNodes();
  LogTestController::getInstance().reset();
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(Configure::nifi_metrics_publisher_metrics, "DeviceInfoNode");
  publisher_->loadMetricNodes();
  std::string expected_log = R"([info] {
    "LogMetrics": {
        "deviceInfo": {
            "identifier":)";
  REQUIRE(utils::verifyLogLinePresenceInPollTime(5s, expected_log));
}

TEST_CASE_METHOD(LogPublisherTestFixture, "Verify generic and publisher specific metric properties", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "100ms");
  SECTION("Only generic metrics are defined") {
    configuration_->set(Configure::nifi_metrics_publisher_metrics, "RepositoryMetrics");
  }
  SECTION("Only publisher specific metrics are defined") {
    configuration_->set(Configure::nifi_metrics_publisher_log_metrics_publisher_metrics, "RepositoryMetrics");
  }
  SECTION("If both generic and publisher specific metrics are defined the publisher specific metrics are used") {
    configuration_->set(Configure::nifi_metrics_publisher_log_metrics_publisher_metrics, "RepositoryMetrics");
    configuration_->set(Configure::nifi_metrics_publisher_metrics, "DeviceInfoNode");
  }
  publisher_->initialize(configuration_, response_node_loader_);
  publisher_->loadMetricNodes();
  REQUIRE(utils::verifyLogMatchesRegexInPollTime(5s, expectedRepositoryMetricsRegex("info")));
}

TEST_CASE_METHOD(LogPublisherTestFixture, "Verify changing log level property for logging", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "100ms");
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_log_level, "dEbUg");
  configuration_->set(Configure::nifi_metrics_publisher_metrics, "RepositoryMetrics");
  publisher_->initialize(configuration_, response_node_loader_);
  publisher_->loadMetricNodes();
  REQUIRE(utils::verifyLogMatchesRegexInPollTime(5s, expectedRepositoryMetricsRegex("debug")));
}

}  // namespace org::apache::nifi::minifi::test
