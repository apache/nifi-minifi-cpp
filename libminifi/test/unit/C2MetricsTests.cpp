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
#include <uuid/uuid.h>
#include <memory>
#include "test/TestBase.h"
#include "io/ClientSocket.h"
#include "core/Processor.h"
#include "core/ClassLoader.h"
#include "core/yaml/YamlConfiguration.h"
#include "core/state/metrics/ProcessMetrics.h"
#include "core/state/metrics/RepositoryMetrics.h"
#include "core/state/metrics/QueueMetrics.h"
#include "core/state/metrics/SystemMetrics.h"

TEST_CASE("TestProcessMetrics", "[c2m1]") {
  minifi::state::metrics::ProcessMetrics metrics;

  REQUIRE("ProcessMetrics" == metrics.getName());

  REQUIRE(2 == metrics.serialize().size());

  REQUIRE("MemoryMetrics" == metrics.serialize().at(0).name);
  REQUIRE("CpuMetrics" == metrics.serialize().at(1).name);
}

TEST_CASE("TestSystemMetrics", "[c2m5]") {
  minifi::state::metrics::SystemInformation metrics;

  REQUIRE("SystemInformation" == metrics.getName());

  REQUIRE(3 == metrics.serialize().size());

  REQUIRE("vcores" == metrics.serialize().at(0).name);
  REQUIRE("physicalmem" == metrics.serialize().at(1).name);
  REQUIRE("machinearch" == metrics.serialize().at(2).name);
}

TEST_CASE("QueueMetricsTestNoConnections", "[c2m2]") {
  minifi::state::metrics::QueueMetrics metrics;

  REQUIRE("QueueMetrics" == metrics.getName());

  REQUIRE(0 == metrics.serialize().size());
}

TEST_CASE("QueueMetricsTestConnections", "[c2m3]") {
  minifi::state::metrics::QueueMetrics metrics;

  REQUIRE("QueueMetrics" == metrics.getName());

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  content_repo->initialize(configuration);

  std::shared_ptr<core::Repository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "testconnection");

  metrics.addConnection(connection);

  connection->setMaxQueueDataSize(1024);
  connection->setMaxQueueSize(1024);

  REQUIRE(1 == metrics.serialize().size());

  minifi::state::metrics::MetricResponse resp = metrics.serialize().at(0);

  REQUIRE("testconnection" == resp.name);

  REQUIRE(4 == resp.children.size());

  minifi::state::metrics::MetricResponse datasize = resp.children.at(0);

  REQUIRE("datasize" == datasize.name);
  REQUIRE("0" == datasize.value);

  minifi::state::metrics::MetricResponse datasizemax = resp.children.at(1);

  REQUIRE("datasizemax" == datasizemax.name);
  REQUIRE("1024" == datasizemax.value);

  minifi::state::metrics::MetricResponse queued = resp.children.at(2);

  REQUIRE("queued" == queued.name);
  REQUIRE("0" == queued.value);

  minifi::state::metrics::MetricResponse queuedmax = resp.children.at(3);

  REQUIRE("queuedmax" == queuedmax.name);
  REQUIRE("1024" == queuedmax.value);
}

TEST_CASE("RepositorymetricsNoRepo", "[c2m4]") {
  minifi::state::metrics::RepositoryMetrics metrics;

  REQUIRE("RepositoryMetrics" == metrics.getName());

  REQUIRE(0 == metrics.serialize().size());
}

TEST_CASE("RepositorymetricsHaveRepo", "[c2m4]") {
  minifi::state::metrics::RepositoryMetrics metrics;

  REQUIRE("RepositoryMetrics" == metrics.getName());

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  metrics.addRepository(repo);
  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::metrics::MetricResponse resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);

    REQUIRE(3 == resp.children.size());

    minifi::state::metrics::MetricResponse running = resp.children.at(0);

    REQUIRE("running" == running.name);
    REQUIRE("0" == running.value);

    minifi::state::metrics::MetricResponse full = resp.children.at(1);

    REQUIRE("full" == full.name);
    REQUIRE("0" == full.value);

    minifi::state::metrics::MetricResponse size = resp.children.at(2);

    REQUIRE("size" == size.name);
    REQUIRE("0" == size.value);
  }

  repo->start();
  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::metrics::MetricResponse resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);

    REQUIRE(3 == resp.children.size());

    minifi::state::metrics::MetricResponse running = resp.children.at(0);

    REQUIRE("running" == running.name);
    REQUIRE("1" == running.value);

    minifi::state::metrics::MetricResponse full = resp.children.at(1);

    REQUIRE("full" == full.name);
    REQUIRE("0" == full.value);

    minifi::state::metrics::MetricResponse size = resp.children.at(2);

    REQUIRE("size" == size.name);
    REQUIRE("0" == size.value);
  }

  repo->setFull();

  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::metrics::MetricResponse resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);

    REQUIRE(3 == resp.children.size());

    minifi::state::metrics::MetricResponse running = resp.children.at(0);

    REQUIRE("running" == running.name);
    REQUIRE("1" == running.value);

    minifi::state::metrics::MetricResponse full = resp.children.at(1);

    REQUIRE("full" == full.name);
    REQUIRE("1" == full.value);

    minifi::state::metrics::MetricResponse size = resp.children.at(2);

    REQUIRE("size" == size.name);
    REQUIRE("0" == size.value);
  }

  repo->stop();

  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::metrics::MetricResponse resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);

    REQUIRE(3 == resp.children.size());

    minifi::state::metrics::MetricResponse running = resp.children.at(0);

    REQUIRE("running" == running.name);
    REQUIRE("0" == running.value);

    minifi::state::metrics::MetricResponse full = resp.children.at(1);

    REQUIRE("full" == full.name);
    REQUIRE("1" == full.value);

    minifi::state::metrics::MetricResponse size = resp.children.at(2);

    REQUIRE("size" == size.name);
    REQUIRE("0" == size.value);
  }
}
