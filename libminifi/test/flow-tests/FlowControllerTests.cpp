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
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "core/Core.h"
#include "core/repository/AtomicRepoEntries.h"
#include "core/RepositoryFactory.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "CustomProcessors.h"
#include "unit/TestControllerWithFlow.h"
#include "unit/EmptyFlow.h"
#include "unit/TestUtils.h"

using namespace std::literals::chrono_literals;

const char* yamlConfig =
    R"(
Flow Controller:
    name: MiNiFi Flow
    id: 2438e3c8-015a-1000-79ca-83af40ec1990
Processors:
  - name: Generator
    id: 2438e3c8-015a-1000-79ca-83af40ec1991
    class: org.apache.nifi.processors.standard.TestFlowFileGenerator
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      Batch Size: 3
  - name: TestProcessor
    id: 2438e3c8-015a-1000-79ca-83af40ec1992
    class: org.apache.nifi.processors.standard.TestProcessor
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 3 sec
    yield period: 1 sec
    run duration nanos: 0
    auto-terminated relationships list:
      - apple
      - banana
Connections:
  - name: Gen
    id: 2438e3c8-015a-1000-79ca-83af40ec1997
    source name: Generator
    source id: 2438e3c8-015a-1000-79ca-83af40ec1991
    source relationship name: success
    destination name: TestProcessor
    destination id: 2438e3c8-015a-1000-79ca-83af40ec1992
    max work queue size: 0
    max work queue data size: 1 MB
    flowfile expiration: 60 sec
Remote Processing Groups:

Controller Services:
  - name: defaultstatestorage
    id: 2438e3c8-015a-1000-79ca-83af40ec1995
    class: PersistentMapStateStorage
    Properties:
      Auto Persistence Interval:
          - value: 0 sec
      File:
          - value: flowcontrollertests_state.txt
)";

// Same shape as yamlConfig, plus a reporting task. Reporting tasks are wrapped in a
// core::Processor and added to the root group, so they show up in getAllProcessors().
const char* yamlConfigWithReportingTask =
    R"(
Flow Controller:
    name: MiNiFi Flow
    id: 2438e3c8-015a-1000-79ca-83af40ec1990
Processors:
  - name: Generator
    id: 2438e3c8-015a-1000-79ca-83af40ec1991
    class: org.apache.nifi.processors.standard.TestFlowFileGenerator
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      Batch Size: 3
Connections:
Remote Processing Groups:
Reporting Tasks:
  - name: ProvenanceReporter
    id: 2438e3c8-015a-1000-79ca-83af40ec1998
    class: org.apache.nifi.minifi.core.reporting.SiteToSiteProvenanceReportingTask
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 1000 ms
Controller Services:
)";

// A nested process group wired through an input port and an output port, which is what
// MiNiFi actually uses ports for.
const char* yamlConfigWithNestedPorts =
    R"(
MiNiFi Config Version: 3
Flow Controller:
  name: MiNiFi Flow
  id: 2438e3c8-015a-1000-79ca-83af40ec1990
Processors: []
Connections: []
Controller Services: []
Remote Process Groups: []
Process Groups:
  - id: 0a3aaf32-8574-4fa7-b720-84001f8dd71a
    name: Nested
    Processors:
      - id: 11624e01-baca-4590-bb9d-512ae2616615
        name: UpdateAttribute
        class: org.apache.nifi.minifi.processors.UpdateAttribute
        scheduling strategy: EVENT_DRIVEN
        auto-terminated relationships list:
        - failure
        Properties:
          test_attribute: success
    Input Ports:
      - id: 012fc536-3137-4360-be65-3e3b47e05941
        name: in
    Output Ports:
      - id: 46dd8c65-8255-4980-8b7e-4381da00867a
        name: out
    Connections:
      - name: in/success/UpdateAttribute
        id: 2d33779c-2305-4e1a-88b8-1d2b6a9b134c
        source id: 012fc536-3137-4360-be65-3e3b47e05941
        source relationship names:
        - success
        destination id: 11624e01-baca-4590-bb9d-512ae2616615
      - name: UpdateAttribute/success/out
        id: 5af95cc8-455a-4d5e-afbb-e16699407ed2
        source id: 11624e01-baca-4590-bb9d-512ae2616615
        source relationship names:
        - success
        destination id: 46dd8c65-8255-4980-8b7e-4381da00867a
)";

template<typename Fn>
bool verifyWithBusyWait(std::chrono::milliseconds timeout, const Fn& fn) {
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < timeout) {
    if (fn()) {
      return true;
    }
  }
  return false;
}

TEST_CASE("Flow shutdown drains connections", "[TestFlow1]") {
  TestControllerWithFlow testController(yamlConfig);
  auto controller = testController.controller_;
  auto root = testController.root_;

  testController.configuration_->set(minifi::Configure::nifi_flowcontroller_drain_timeout, "100 ms");

  TypedProcessorWrapper<minifi::processors::TestProcessor> sinkProc = root->findProcessorByName("TestProcessor");
  gsl_Assert(sinkProc);
  // prevent execution of the consumer processor
  sinkProc->yield(10s);

  std::map<std::string, minifi::Connection*> connectionMap;

  root->getConnections(connectionMap);
  // adds the single connection to the map both by name and id
  REQUIRE(connectionMap.size() == 2);

  testController.startFlow();

  // wait for the generator to create some files
  std::this_thread::sleep_for(std::chrono::milliseconds{1000});

  for (auto& it : connectionMap) {
    REQUIRE(it.second->getQueueSize() > 10);
  }

  controller->stop();

  REQUIRE(sinkProc.get().trigger_count == 0);

  for (auto& it : connectionMap) {
    REQUIRE(it.second->isEmpty());
  }
}

TEST_CASE("Flow shutdown waits for a while", "[TestFlow2]") {
  TestControllerWithFlow testController(yamlConfig);
  auto controller = testController.controller_;
  auto root = testController.root_;

  testController.configuration_->set(minifi::Configure::nifi_flowcontroller_drain_timeout, "10 s");

  TypedProcessorWrapper<minifi::processors::TestFlowFileGenerator> sourceProc = root->findProcessorByName("Generator");
  TypedProcessorWrapper<minifi::processors::TestProcessor> sinkProc = root->findProcessorByName("TestProcessor");

  std::promise<void> execSinkPromise;
  std::future<void> execSinkFuture = execSinkPromise.get_future();
  sinkProc.get().onTriggerCb_ = [&] {
    execSinkFuture.wait();
  };

  testController.startFlow();

  // wait for the source processor to enqueue its flowFiles
  auto flowFilesEnqueued = [&] { return root->getTotalFlowFileCount() >= 3; };
  REQUIRE(verifyWithBusyWait(std::chrono::milliseconds{500}, flowFilesEnqueued));

  REQUIRE(sourceProc.get().trigger_count.load() >= 1);

  execSinkPromise.set_value();
  controller->stop();

  REQUIRE(sourceProc.get().trigger_count.load() >= 1);
  REQUIRE(sinkProc.get().trigger_count.load() >= 3);
}

TEST_CASE("Flow stopped after grace period", "[TestFlow3]") {
  TestControllerWithFlow testController(yamlConfig);
  auto controller = testController.controller_;
  auto root = testController.root_;

  testController.configuration_->set(minifi::Configure::nifi_flowcontroller_drain_timeout, "1000 ms");

  TypedProcessorWrapper<minifi::processors::TestFlowFileGenerator> sourceProc = root->findProcessorByName("Generator");
  TypedProcessorWrapper<minifi::processors::TestProcessor> sinkProc = root->findProcessorByName("TestProcessor");

  std::promise<void> execSinkPromise;
  std::future<void> execSinkFuture = execSinkPromise.get_future();
  sinkProc.get().onTriggerCb_ = [&]{
    execSinkFuture.wait();
    static std::atomic<bool> first_onTrigger{true};
    bool isFirst = true;
    // sleep only on the first trigger
    if (first_onTrigger.compare_exchange_strong(isFirst, false)) {
      std::this_thread::sleep_for(std::chrono::milliseconds{1500});
    }
  };

  testController.startFlow();

  // wait for the source processor to enqueue its flowFiles
  auto flowFilesEnqueued = [&] { return root->getTotalFlowFileCount() >= 3; };
  REQUIRE(verifyWithBusyWait(std::chrono::milliseconds{500}, flowFilesEnqueued));

  REQUIRE(sourceProc.get().trigger_count.load() >= 1);

  execSinkPromise.set_value();
  controller->stop();

  REQUIRE(sourceProc.get().trigger_count.load() >= 1);
  REQUIRE(sinkProc.get().trigger_count.load() >= 1);
}

TEST_CASE("Extend the waiting period during shutdown", "[TestFlow4]") {
  TestControllerWithFlow testController(yamlConfig);
  auto controller = testController.controller_;
  auto root = testController.root_;

  std::chrono::milliseconds timeout_ms = 1000ms;

  testController.configuration_->set(minifi::Configure::nifi_flowcontroller_drain_timeout, fmt::format("{}", timeout_ms));

  TypedProcessorWrapper<minifi::processors::TestFlowFileGenerator> sourceProc = root->findProcessorByName("Generator");
  TypedProcessorWrapper<minifi::processors::TestProcessor> sinkProc = root->findProcessorByName("TestProcessor");

  std::promise<void> execSinkPromise;
  std::future<void> execSinkFuture = execSinkPromise.get_future();
  sinkProc.get().onTriggerCb_ = [&]{
    execSinkFuture.wait();
    static std::atomic<bool> first_onTrigger{true};
    bool isFirst = true;
    // sleep only on the first trigger
    if (first_onTrigger.compare_exchange_strong(isFirst, false)) {
      std::this_thread::sleep_for(std::chrono::milliseconds{1500});
    }
  };

  testController.startFlow();

  // wait for the source processor to enqueue its flowFiles
  auto flowFilesEnqueued = [&] { return root->getTotalFlowFileCount() >= 3; };
  REQUIRE(verifyWithBusyWait(std::chrono::milliseconds{500}, flowFilesEnqueued));

  REQUIRE(sourceProc.get().trigger_count.load() >= 1);

  std::thread shutdownThread([&]{
    execSinkPromise.set_value();
    controller->stop();
  });

  auto shutdownInitiated = std::chrono::steady_clock::now();
  auto shutdownDuration = [&] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - shutdownInitiated);};

  std::this_thread::sleep_for(std::chrono::milliseconds{500});
  while (shutdownDuration() < std::chrono::milliseconds(5000) && controller->isRunning()) {
    timeout_ms += 500ms;
    testController.getLogger()->log_info("Controller still running after {}, extending the waiting period to {}, ff count: {}",
        shutdownDuration(), timeout_ms, static_cast<unsigned int>(root->getTotalFlowFileCount()));
    testController.configuration_->set(minifi::Configure::nifi_flowcontroller_drain_timeout, fmt::format("{}", timeout_ms));
    std::this_thread::sleep_for(std::chrono::milliseconds{500});
  }

  REQUIRE(!controller->isRunning());
  REQUIRE(shutdownDuration().count() > 1500);

  shutdownThread.join();

  REQUIRE(sourceProc.get().trigger_count.load() >= 1);
  REQUIRE(sinkProc.get().trigger_count.load() >= 3);
}

TEST_CASE("FlowController::getFlowTopology exposes processors and connections", "[FlowTopology]") {
  TestControllerWithFlow testController(yamlConfig);
  auto topology = testController.controller_->getFlowTopology();

  REQUIRE(topology.root_group_name == "MiNiFi Flow");
  REQUIRE(topology.root_group_uuid.to_string().view() == std::string_view{"2438e3c8-015a-1000-79ca-83af40ec1990"});

  REQUIRE(topology.processors.size() == 2);
  const auto gen = std::ranges::find_if(topology.processors, [](const auto& p) { return p.name == "Generator"; });
  const auto tst = std::ranges::find_if(topology.processors, [](const auto& p) { return p.name == "TestProcessor"; });
  REQUIRE(gen != topology.processors.end());
  REQUIRE(tst != topology.processors.end());
  REQUIRE(gen->uuid.to_string().view() == std::string_view{"2438e3c8-015a-1000-79ca-83af40ec1991"});
  REQUIRE(gen->type == "TestFlowFileGenerator");

  REQUIRE(topology.connections.size() == 1);
  const auto& conn = topology.connections.front();
  REQUIRE(conn.name == "Gen");
  REQUIRE(conn.source_uuid == gen->uuid);
  REQUIRE(conn.destination_uuid == tst->uuid);
  REQUIRE(conn.relationships.size() == 1);
  REQUIRE(conn.relationships.front() == "success");
}

TEST_CASE("FlowController::getFlowTopology excludes reporting tasks", "[FlowTopology]") {
  TestControllerWithFlow testController(yamlConfigWithReportingTask);
  const auto topology = testController.controller_->getFlowTopology();

  // The reporting task is wrapped in a core::Processor and lives in the root group's
  // processor list, but it is not part of the dataflow, so it must not be reported.
  REQUIRE(topology.processors.size() == 1);
  REQUIRE(topology.processors.front().name == "Generator");
  REQUIRE(std::ranges::none_of(topology.processors, [](const auto& processor) {
    return processor.name == "ProvenanceReporter" || processor.type == "SiteToSiteProvenanceReportingTask";
  }));
}

TEST_CASE("FlowController::getFlowTopology reports ports as ordinary processors", "[FlowTopology]") {
  TestControllerWithFlow testController(yamlConfigWithNestedPorts);
  const auto topology = testController.controller_->getFlowTopology();

  // A Port is a ForwardingNode at runtime, exactly like a Funnel, and MiNiFi has no
  // site-to-site server, so a port is never an external interface. Reporting it as a
  // node keeps the path through it continuous instead of splitting it in two.
  REQUIRE(topology.processors.size() == 3);

  const auto by_name = [&topology](std::string_view name) {
    return std::ranges::find_if(topology.processors, [name](const auto& processor) { return processor.name == name; });
  };
  const auto in = by_name("in");
  const auto out = by_name("out");
  const auto update = by_name("UpdateAttribute");
  REQUIRE(in != topology.processors.end());
  REQUIRE(out != topology.processors.end());
  REQUIRE(update != topology.processors.end());
  REQUIRE(in->type == "Port");
  REQUIRE(out->type == "Port");
  REQUIRE(update->type == "UpdateAttribute");

  // Both connections survive with the ports as endpoints, so the chain is walkable.
  REQUIRE(topology.connections.size() == 2);
  REQUIRE(std::ranges::any_of(topology.connections, [&](const auto& connection) {
    return connection.source_uuid == in->uuid && connection.destination_uuid == update->uuid;
  }));
  REQUIRE(std::ranges::any_of(topology.connections, [&](const auto& connection) {
    return connection.source_uuid == update->uuid && connection.destination_uuid == out->uuid;
  }));
}

TEST_CASE("FlowController::getFlowTopology is stable across calls", "[FlowTopology]") {
  TestControllerWithFlow testController(yamlConfig);

  // Reporting tasks compare successive snapshots to decide whether the flow actually
  // changed, so two snapshots of an unchanged flow must be equal. getAllProcessors and
  // getConnections give no order guarantee; getFlowTopology sorts to compensate.
  const auto first = testController.controller_->getFlowTopology();
  const auto second = testController.controller_->getFlowTopology();
  REQUIRE(first == second);

  // ...and a genuine difference must be detected. Sanity-check that operator== is not
  // trivially true by perturbing each vector-valued member in turn.
  auto changed_processor = first;
  changed_processor.processors.at(0).name += "-renamed";
  REQUIRE(first != changed_processor);

  auto changed_connection = first;
  changed_connection.connections.at(0).relationships.emplace_back("failure");
  REQUIRE(first != changed_connection);

  auto removed_processor = first;
  removed_processor.processors.pop_back();
  REQUIRE(first != removed_processor);

  auto added_processor = first;
  added_processor.processors.push_back({.uuid = minifi::utils::IdGenerator::getIdGenerator()->generate(), .name = "in", .type = "Port"});
  REQUIRE(first != added_processor);
}

TEST_CASE("FlowController destructor releases resources", "[TestFlow5]") {
  TestControllerWithFlow controller(R"(
Flow Controller:
  name: Banana Bread
Processors:
- name: GenFF
  id: 00000000-0000-0000-0000-000000000001
  class: GenerateFlowFile
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 10 sec
Connections: []
Remote Processing Groups: []
Controller Services: []
)");

  controller.startFlow();

  REQUIRE(LogTestController::getInstance().countOccurrences("Creating scheduling agent") == 3);
  LogTestController::getInstance().clear();

  bool update_successful = controller.controller_->applyConfiguration("/flows/1", empty_flow).has_value();
  REQUIRE(update_successful);

  REQUIRE(LogTestController::getInstance().countOccurrences("Creating scheduling agent") == 3);
  REQUIRE(LogTestController::getInstance().countOccurrences("Destroying scheduling agent") == 3);
  LogTestController::getInstance().clear();

  // manually destroy the controller
  controller.controller_.reset();

  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Destroying FlowController"));
  REQUIRE(LogTestController::getInstance().countOccurrences("Destroying scheduling agent") == 3);
}
