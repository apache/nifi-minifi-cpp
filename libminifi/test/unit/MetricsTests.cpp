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

#include "../../include/core/state/nodes/QueueMetrics.h"
#include "../../include/core/state/nodes/RepositoryMetrics.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/Processor.h"
#include "core/ClassLoader.h"
#include "repository/VolatileContentRepository.h"
#include "unit/ProvenanceTestHelper.h"
#include "unit/DummyProcessor.h"
#include "range/v3/algorithm/find_if.hpp"
#include "core/ProcessorMetrics.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

void checkSerializedValue(const std::vector<org::apache::nifi::minifi::state::response::SerializedResponseNode>& children, const std::string& name, const std::string& expected_value) {
  auto it = ranges::find_if(children, [&](const auto& child) { return child.name == name; });
  REQUIRE(it != children.end());
  REQUIRE(expected_value == it->value.to_string());
}

TEST_CASE("QueueMetricsTestNoConnections", "[c2m2]") {
  minifi::state::response::QueueMetrics metrics;

  REQUIRE("QueueMetrics" == metrics.getName());
  REQUIRE(metrics.serialize().empty());
}

TEST_CASE("QueueMetricsTestConnections", "[c2m3]") {
  minifi::state::response::QueueMetrics metrics;

  REQUIRE("QueueMetrics" == metrics.getName());

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  content_repo->initialize(configuration);

  std::shared_ptr<core::Repository> repo = std::make_shared<TestRepository>();

  auto connection = std::make_unique<minifi::ConnectionImpl>(repo, content_repo, "testconnection");

  connection->setBackpressureThresholdDataSize(1024);
  connection->setBackpressureThresholdCount(1024);

  metrics.updateConnection(connection.get());

  auto seialized_metrics = metrics.serialize();
  REQUIRE(1 == seialized_metrics.size());

  minifi::state::response::SerializedResponseNode resp = metrics.serialize().at(0);

  REQUIRE("testconnection" == resp.name);
  REQUIRE(4 == resp.children.size());

  checkSerializedValue(resp.children, "datasize", "0");
  checkSerializedValue(resp.children, "datasizemax", "1024");
  checkSerializedValue(resp.children, "queued", "0");
  checkSerializedValue(resp.children, "queuedmax", "1024");
}

TEST_CASE("RepositorymetricsNoRepo", "[c2m4]") {
  minifi::state::response::RepositoryMetrics metrics;

  REQUIRE("RepositoryMetrics" == metrics.getName());
  REQUIRE(metrics.serialize().empty());
}

TEST_CASE("RepositorymetricsHaveRepo", "[c2m4]") {
  minifi::state::response::RepositoryMetrics metrics;

  REQUIRE("RepositoryMetrics" == metrics.getName());

  std::shared_ptr<TestThreadedRepository> repo;
  size_t expected_metric_count{};

  SECTION("Non-RocksDB repository") {
    repo = std::make_shared<TestThreadedRepository>();
    expected_metric_count = 5;
  }

  SECTION("RocksDB repository") {
    repo = std::make_shared<TestRocksDbRepository>();
    expected_metric_count = 7;
  }


  metrics.addRepository(repo);
  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::response::SerializedResponseNode resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);
    REQUIRE(expected_metric_count == resp.children.size());

    checkSerializedValue(resp.children, "running", "false");
    checkSerializedValue(resp.children, "full", "false");
    checkSerializedValue(resp.children, "size", "0");
    checkSerializedValue(resp.children, "maxSize", "0");
    checkSerializedValue(resp.children, "entryCount", "0");
    if (expected_metric_count > 5) {
      checkSerializedValue(resp.children, "rocksDbTableReadersSize", "100");
      checkSerializedValue(resp.children, "rocksDbAllMemoryTablesSize", "200");
    }
  }

  repo->start();
  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::response::SerializedResponseNode resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);
    REQUIRE(expected_metric_count == resp.children.size());

    checkSerializedValue(resp.children, "running", "true");
    checkSerializedValue(resp.children, "full", "false");
    checkSerializedValue(resp.children, "size", "0");
    checkSerializedValue(resp.children, "maxSize", "0");
    checkSerializedValue(resp.children, "entryCount", "0");
    if (expected_metric_count > 5) {
      checkSerializedValue(resp.children, "rocksDbTableReadersSize", "100");
      checkSerializedValue(resp.children, "rocksDbAllMemoryTablesSize", "200");
    }
  }

  repo->stop();

  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::response::SerializedResponseNode resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);
    REQUIRE(expected_metric_count == resp.children.size());

    checkSerializedValue(resp.children, "running", "false");
    checkSerializedValue(resp.children, "full", "false");
    checkSerializedValue(resp.children, "size", "0");
    checkSerializedValue(resp.children, "maxSize", "0");
    checkSerializedValue(resp.children, "entryCount", "0");
    if (expected_metric_count > 5) {
      checkSerializedValue(resp.children, "rocksDbTableReadersSize", "100");
      checkSerializedValue(resp.children, "rocksDbAllMemoryTablesSize", "200");
    }
  }
}

TEST_CASE("VolatileRepositorymetricsCanBeFull", "[c2m4]") {
  minifi::state::response::RepositoryMetrics metrics;

  REQUIRE("RepositoryMetrics" == metrics.getName());

  auto repo = std::make_shared<TestVolatileRepository>();

  metrics.addRepository(repo);
  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::response::SerializedResponseNode resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);
    REQUIRE(5 == resp.children.size());

    checkSerializedValue(resp.children, "running", "false");
    checkSerializedValue(resp.children, "full", "false");
    checkSerializedValue(resp.children, "size", "0");
    checkSerializedValue(resp.children, "maxSize", std::to_string(static_cast<int64_t>(TEST_MAX_REPOSITORY_STORAGE_SIZE * 0.75)));
    checkSerializedValue(resp.children, "entryCount", "0");
  }

  repo->setFull();

  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::response::SerializedResponseNode resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);
    REQUIRE(5 == resp.children.size());

    checkSerializedValue(resp.children, "running", "false");
    checkSerializedValue(resp.children, "full", "true");
    checkSerializedValue(resp.children, "size", std::to_string(static_cast<int64_t>(TEST_MAX_REPOSITORY_STORAGE_SIZE * 0.75)));
    checkSerializedValue(resp.children, "maxSize", std::to_string(static_cast<int64_t>(TEST_MAX_REPOSITORY_STORAGE_SIZE * 0.75)));
    checkSerializedValue(resp.children, "entryCount", "10000");
  }
}

TEST_CASE("Test on trigger runtime processor metrics", "[ProcessorMetrics]") {
  DummyProcessor dummy_processor("dummy");
  minifi::core::ProcessorMetricsImpl metrics(dummy_processor);

  REQUIRE("DummyProcessorMetrics" == metrics.getName());

  REQUIRE(metrics.getLastOnTriggerRuntime() == 0ms);
  REQUIRE(metrics.getAverageOnTriggerRuntime() == 0ms);

  metrics.addLastOnTriggerRuntime(10ms);
  metrics.addLastOnTriggerRuntime(20ms);
  metrics.addLastOnTriggerRuntime(30ms);

  REQUIRE(metrics.getLastOnTriggerRuntime() == 30ms);
  REQUIRE(metrics.getAverageOnTriggerRuntime() == 20ms);

  for (auto i = 0; i < 7; ++i) {
    metrics.addLastOnTriggerRuntime(50ms);
  }
  REQUIRE(metrics.getAverageOnTriggerRuntime() == 41ms);
  REQUIRE(metrics.getLastOnTriggerRuntime() == 50ms);

  for (auto i = 0; i < 3; ++i) {
    metrics.addLastOnTriggerRuntime(50ms);
  }
  REQUIRE(metrics.getAverageOnTriggerRuntime() == 50ms);
  REQUIRE(metrics.getLastOnTriggerRuntime() == 50ms);

  for (auto i = 0; i < 10; ++i) {
    metrics.addLastOnTriggerRuntime(40ms);
  }
  REQUIRE(metrics.getAverageOnTriggerRuntime() == 40ms);
  REQUIRE(metrics.getLastOnTriggerRuntime() == 40ms);

  metrics.addLastOnTriggerRuntime(10ms);
  REQUIRE(metrics.getLastOnTriggerRuntime() == 10ms);
  REQUIRE(metrics.getAverageOnTriggerRuntime() == 37ms);
}

TEST_CASE("Test commit runtime processor metrics", "[ProcessorMetrics]") {
  DummyProcessor dummy_processor("dummy");
  minifi::core::ProcessorMetricsImpl metrics(dummy_processor);

  REQUIRE("DummyProcessorMetrics" == metrics.getName());

  REQUIRE(metrics.getLastSessionCommitRuntime() == 0ms);
  REQUIRE(metrics.getAverageSessionCommitRuntime() == 0ms);

  metrics.addLastSessionCommitRuntime(10ms);
  metrics.addLastSessionCommitRuntime(20ms);
  metrics.addLastSessionCommitRuntime(30ms);

  REQUIRE(metrics.getLastSessionCommitRuntime() == 30ms);
  REQUIRE(metrics.getAverageSessionCommitRuntime() == 20ms);

  for (auto i = 0; i < 7; ++i) {
    metrics.addLastSessionCommitRuntime(50ms);
  }
  REQUIRE(metrics.getAverageSessionCommitRuntime() == 41ms);
  REQUIRE(metrics.getLastSessionCommitRuntime() == 50ms);

  for (auto i = 0; i < 3; ++i) {
    metrics.addLastSessionCommitRuntime(50ms);
  }
  REQUIRE(metrics.getAverageSessionCommitRuntime() == 50ms);
  REQUIRE(metrics.getLastSessionCommitRuntime() == 50ms);

  for (auto i = 0; i < 10; ++i) {
    metrics.addLastSessionCommitRuntime(40ms);
  }
  REQUIRE(metrics.getAverageSessionCommitRuntime() == 40ms);
  REQUIRE(metrics.getLastSessionCommitRuntime() == 40ms);

  metrics.addLastSessionCommitRuntime(10ms);
  REQUIRE(metrics.getLastSessionCommitRuntime() == 10ms);
  REQUIRE(metrics.getAverageSessionCommitRuntime() == 37ms);
}

}  // namespace org::apache::nifi::minifi::test
