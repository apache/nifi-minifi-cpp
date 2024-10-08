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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "c2/ControllerSocketMetricsPublisher.h"
#include "core/state/nodes/MetricsBase.h"
#include "core/state/nodes/QueueMetrics.h"
#include "core/state/nodes/ResponseNodeLoader.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::test {

class TestQueueMetrics : public state::response::ResponseNodeImpl {
 public:
  TestQueueMetrics() {
    metrics_ = {
      {"queue_size", 1, {{"connection_name", "con1"}}},
      {"queue_size_max", 2, {{"connection_name", "con1"}}},
      {"queue_size", 3, {{"connection_name", "con2"}}},
      {"queue_size_max", 3, {{"connection_name", "con2"}}},
    };
  }

  std::vector<state::response::SerializedResponseNode> serialize() override {
    return {};
  }

  std::vector<state::PublishedMetric> calculateMetrics() override {
    return metrics_;
  }

 protected:
  std::vector<state::PublishedMetric> metrics_;
};

class TestControllerSocketMetricsPublisher : public c2::ControllerSocketMetricsPublisher {
 public:
  using ControllerSocketMetricsPublisher::ControllerSocketMetricsPublisher;
  void setQueueMetricsNode(std::shared_ptr<state::response::ResponseNode> node) {
    queue_metrics_node_ = std::move(node);
  }

  [[nodiscard]] std::shared_ptr<org::apache::nifi::minifi::state::response::ResponseNode> getQueueMetricsNode() const {
    return queue_metrics_node_;
  }
};

class ControllerSocketMetricsPublisherTestFixture {
 public:
  ControllerSocketMetricsPublisherTestFixture()
      : configuration_(std::make_shared<ConfigureImpl>()),
        response_node_loader_(std::make_shared<state::response::ResponseNodeLoaderImpl>(configuration_, std::vector<std::shared_ptr<core::RepositoryMetricsSource>>{}, nullptr)),
        test_response_node_(std::make_shared<TestQueueMetrics>()),
        controller_socket_metrics_publisher_("test_publisher") {
    controller_socket_metrics_publisher_.initialize(configuration_, response_node_loader_);
  }

 protected:
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<state::response::ResponseNodeLoader> response_node_loader_;
  std::shared_ptr<TestQueueMetrics> test_response_node_;
  TestControllerSocketMetricsPublisher controller_socket_metrics_publisher_;
};

TEST_CASE_METHOD(ControllerSocketMetricsPublisherTestFixture, "Load and clear", "[ControllerSocketMetricsPublisher]") {
  controller_socket_metrics_publisher_.loadMetricNodes();
  auto node = controller_socket_metrics_publisher_.getQueueMetricsNode();
  auto queue_metrics = dynamic_cast<state::response::QueueMetrics*>(node.get());
  REQUIRE(queue_metrics);
  controller_socket_metrics_publisher_.clearMetricNodes();
  REQUIRE(!controller_socket_metrics_publisher_.getQueueMetricsNode());
}

TEST_CASE_METHOD(ControllerSocketMetricsPublisherTestFixture, "Test getting queue sizes", "[ControllerSocketMetricsPublisher]") {
  auto sizes = controller_socket_metrics_publisher_.getQueueSizes();
  REQUIRE(sizes.empty());
  controller_socket_metrics_publisher_.setQueueMetricsNode(test_response_node_);
  sizes = controller_socket_metrics_publisher_.getQueueSizes();
  REQUIRE(sizes.size() == 2);
  CHECK(sizes["con1"].queue_size == 1);
  CHECK(sizes["con1"].queue_size_max == 2);
  CHECK(sizes["con2"].queue_size == 3);
  CHECK(sizes["con2"].queue_size_max == 3);
}

TEST_CASE_METHOD(ControllerSocketMetricsPublisherTestFixture, "Test getting full connections", "[ControllerSocketMetricsPublisher]") {
  auto full_connections = controller_socket_metrics_publisher_.getFullConnections();
  REQUIRE(full_connections.empty());
  controller_socket_metrics_publisher_.setQueueMetricsNode(test_response_node_);
  full_connections = controller_socket_metrics_publisher_.getFullConnections();
  REQUIRE(full_connections.size() == 1);
  REQUIRE(full_connections.contains("con2"));
}

TEST_CASE_METHOD(ControllerSocketMetricsPublisherTestFixture, "Test getting connections", "[ControllerSocketMetricsPublisher]") {
  auto connections = controller_socket_metrics_publisher_.getConnections();
  REQUIRE(connections.empty());
  controller_socket_metrics_publisher_.setQueueMetricsNode(test_response_node_);
  connections = controller_socket_metrics_publisher_.getConnections();
  REQUIRE(connections.size() == 2);
  REQUIRE(connections.contains("con1"));
  REQUIRE(connections.contains("con2"));
}

TEST_CASE_METHOD(ControllerSocketMetricsPublisherTestFixture, "Test getting manifest", "[ControllerSocketMetricsPublisher]") {
  auto agent_manifest = controller_socket_metrics_publisher_.getAgentManifest();
  CHECK(agent_manifest.find("\"agentManifestHash\":") != std::string::npos);
  CHECK(agent_manifest.find("\"agentManifest\":") != std::string::npos);
  CHECK(agent_manifest.find("\"buildInfo\":") != std::string::npos);
  CHECK(agent_manifest.find("\"bundles\": [") != std::string::npos);
  CHECK(agent_manifest.find("\"componentManifest\": {") != std::string::npos);
  CHECK(agent_manifest.find("\"supportedOperations\": [") != std::string::npos);
  CHECK(agent_manifest.find("\"type\": \"describe\"") != std::string::npos);
  CHECK(agent_manifest.find("\"availableProperties\": [") != std::string::npos);
  CHECK(agent_manifest.find("\"agentType\": \"cpp\"") != std::string::npos);
}

}  // namespace org::apache::nifi::minifi::test
