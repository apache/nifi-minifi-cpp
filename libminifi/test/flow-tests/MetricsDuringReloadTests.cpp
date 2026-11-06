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
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

#include "c2/C2MetricsPublisher.h"
#include "c2/C2Utils.h"
#include "core/state/MetricsPublisherStore.h"
#include "properties/Configuration.h"
#include "CustomProcessors.h"
#include "unit/Catch.h"
#include "unit/TestControllerWithFlow.h"

namespace {

constexpr std::string_view SinkProcessorName = "Sink Processor";

std::string createFlow(const std::string& generator_processor_name) {
  return R"(
Flow Controller:
  name: MiNiFi Flow
Processors:
- name: )" + generator_processor_name + R"(
  id: 2438e3c8-015a-1000-79ca-83af40ec1991
  class: org.apache.nifi.processors.standard.TestFlowFileGenerator
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 10 ms
  auto-terminated relationships list:
  Properties:
    Batch Size: 1
- name: )" + std::string{SinkProcessorName} + R"(
  id: 2438e3c8-015a-1000-79ca-83af40ec1992
  class: org.apache.nifi.processors.standard.TestProcessor
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 10 ms
  auto-terminated relationships list:
  - apple
  - banana
Connections:
- name: GeneratorToSink
  id: 2438e3c8-015a-1000-79ca-83af40ec1997
  source name: )" + generator_processor_name + R"(
  source id: 2438e3c8-015a-1000-79ca-83af40ec1991
  source relationship name: success
  destination name: )" + std::string{SinkProcessorName} + R"(
  destination id: 2438e3c8-015a-1000-79ca-83af40ec1992
  max work queue size: 0
  max work queue data size: 1 MB
  flowfile expiration: 60 sec
Remote Processing Groups: []
Controller Services: []
)";
}

constexpr std::string_view UnparseableFlow = "this is not a valid flow configuration";

constexpr std::array DescribedClasses = {"FlowInformation", "QueueMetrics", "processorMetrics/.*"};

std::unique_ptr<TestControllerWithFlow> createTestController(const std::string& flow) {
  auto test_controller = std::make_unique<TestControllerWithFlow>(flow.c_str(), /* setup_flow = */ false);
  test_controller->configuration_->set(minifi::Configuration::nifi_c2_root_classes,
      "DeviceInfoNode,AgentInformation,FlowInformation,QueueMetrics");
  test_controller->setupFlow();
  return test_controller;
}

std::unordered_set<std::string> processorNames(const std::vector<minifi::state::ComponentState>& components) {
  REQUIRE_FALSE(components.empty());
  CHECK(components.back().name == "FlowController");
  return components | std::views::take(components.size() - 1)
      | std::views::transform([](const auto& component) { return component.name; })
      | std::ranges::to<std::unordered_set>();
}

}  // namespace

TEST_CASE("Metrics can be collected while the flow is reloaded", "[MetricsDuringReload]") {
  auto test_controller = createTestController(createFlow("Processor One"));
  test_controller->startFlow();

  auto* metrics_publisher_store = test_controller->metrics_publisher_store_;
  REQUIRE(metrics_publisher_store != nullptr);
  auto c2_metrics_publisher = std::dynamic_pointer_cast<minifi::c2::C2MetricsPublisher>(
      metrics_publisher_store->getMetricsPublisher(minifi::c2::C2_METRICS_PUBLISHER).lock());
  REQUIRE(c2_metrics_publisher != nullptr);
  REQUIRE_FALSE(c2_metrics_publisher->getHeartbeatNodes(true).empty());

  std::atomic<bool> reloads_done{false};
  std::atomic<size_t> heartbeat_nodes{0};
  std::atomic<size_t> described_nodes{0};

  // Readers: the heartbeat and the describe path, i.e. what the C2 agent's two worker threads do. A failure
  // here shows up as a hang, a TSan report or an ASan report rather than a failed assertion, because REQUIRE
  // is not safe to call from a non-Catch2 thread.
  auto heartbeat_thread = std::thread([&] {
    while (!reloads_done) {
      heartbeat_nodes += c2_metrics_publisher->getHeartbeatNodes(false).size();
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
  });
  auto describe_thread = std::thread([&] {
    for (size_t i = 0; !reloads_done; ++i) {
      if (const auto node = c2_metrics_publisher->getMetricsNode(DescribedClasses[i % DescribedClasses.size()])) {
        described_nodes += node->serialized_nodes.size();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
  });

  // Writer: alternate between two flows. The unparseable payload is rejected before the live flow is
  // touched, which must leave the running flow and the readers alone.
  size_t successful_reloads = 0;
  for (int i = 0; i < 12; ++i) {
    if (i % 4 == 3) {
      CHECK_FALSE(test_controller->controller_->applyConfiguration("/flows/bad", std::string{UnparseableFlow}).has_value());
    } else if (test_controller->controller_->applyConfiguration(
        "/flows/" + std::to_string(i), createFlow(i % 2 == 0 ? "Processor Two" : "Processor One")).has_value()) {
      ++successful_reloads;
    }
  }
  reloads_done = true;
  heartbeat_thread.join();
  describe_thread.join();

  CHECK(successful_reloads == 9);
  CHECK(heartbeat_nodes > 0);
  CHECK(described_nodes > 0);

  // The flow survived all that and still reports itself.
  CHECK(processorNames(test_controller->controller_->getComponentStates())
      == std::unordered_set<std::string>{"Processor Two", std::string{SinkProcessorName}});
}

TEST_CASE("getComponentStates lists every processor and then the agent", "[MetricsDuringReload]") {
  auto test_controller = createTestController(createFlow("Processor One"));
  auto& flow_controller = *test_controller->controller_;

  const std::unordered_set<std::string> flow_one{"Processor One", std::string{SinkProcessorName}};

  SECTION("before the flow is started") {
    const auto components = flow_controller.getComponentStates();
    CHECK(processorNames(components) == flow_one);
    CHECK_FALSE(components.back().running);
  }

  SECTION("after the flow is started") {
    test_controller->startFlow();
    const auto components = flow_controller.getComponentStates();
    CHECK(processorNames(components) == flow_one);
    CHECK(components.back().running);
  }

  const std::unordered_set<std::string> flow_two{"Processor Two", std::string{SinkProcessorName}};

  SECTION("the processor list follows a flow update") {
    test_controller->startFlow();
    REQUIRE(flow_controller.applyConfiguration("/flows/2", createFlow("Processor Two")).has_value());
    const auto components = flow_controller.getComponentStates();
    CHECK(processorNames(components) == flow_two);
  }

  SECTION("a rejected flow update leaves the component list alone") {
    test_controller->startFlow();
    REQUIRE_FALSE(flow_controller.applyConfiguration("/flows/bad", std::string{UnparseableFlow}).has_value());
    const auto components = flow_controller.getComponentStates();
    CHECK(processorNames(components) == flow_one);
  }
}
