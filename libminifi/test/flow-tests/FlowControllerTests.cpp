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
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "core/Core.h"
#include "core/repository/AtomicRepoEntries.h"
#include "core/RepositoryFactory.h"
#include "FlowFileRecord.h"
#include "provenance/Provenance.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "../TestBase.h"
#include "YamlConfiguration.h"
#include "CustomProcessors.h"
#include "TestControllerWithFlow.h"

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
  - name: defaultstatemanagerprovider
    id: 2438e3c8-015a-1000-79ca-83af40ec1995
    class: UnorderedMapPersistableKeyValueStoreService
    Properties:
      Auto Persistence Interval:
          - value: 0 sec
      File:
          - value: flowcontrollertests_state.txt
)";

template<typename Fn>
bool verifyWithBusyWait(std::chrono::milliseconds timeout, Fn&& fn) {
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

  auto sinkProc = std::static_pointer_cast<minifi::processors::TestProcessor>(root->findProcessorByName("TestProcessor"));
  // prevent execution of the consumer processor
  sinkProc->yield(10000);

  std::map<std::string, std::shared_ptr<minifi::Connection>> connectionMap;

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

  REQUIRE(sinkProc->trigger_count == 0);

  for (auto& it : connectionMap) {
    REQUIRE(it.second->isEmpty());
  }
}

TEST_CASE("Flow shutdown waits for a while", "[TestFlow2]") {
  TestControllerWithFlow testController(yamlConfig);
  auto controller = testController.controller_;
  auto root = testController.root_;

  testController.configuration_->set(minifi::Configure::nifi_flowcontroller_drain_timeout, "10 s");

  auto sourceProc = std::static_pointer_cast<minifi::processors::TestFlowFileGenerator>(root->findProcessorByName("Generator"));
  auto sinkProc = std::static_pointer_cast<minifi::processors::TestProcessor>(root->findProcessorByName("TestProcessor"));

  std::promise<void> execSinkPromise;
  std::future<void> execSinkFuture = execSinkPromise.get_future();
  sinkProc->onTriggerCb_ = [&] {
    execSinkFuture.wait();
  };

  testController.startFlow();

  // wait for the source processor to enqueue its flowFiles
  auto flowFilesEnqueued = [&] {return root->getTotalFlowFileCount() == 3;};
  REQUIRE(verifyWithBusyWait(std::chrono::milliseconds{200}, flowFilesEnqueued));

  REQUIRE(sourceProc->trigger_count.load() == 1);

  execSinkPromise.set_value();
  controller->stop();

  REQUIRE(sourceProc->trigger_count.load() == 1);
  REQUIRE(sinkProc->trigger_count.load() == 3);
}

TEST_CASE("Flow stopped after grace period", "[TestFlow3]") {
  TestControllerWithFlow testController(yamlConfig);
  auto controller = testController.controller_;
  auto root = testController.root_;

  testController.configuration_->set(minifi::Configure::nifi_flowcontroller_drain_timeout, "1000 ms");

  auto sourceProc = std::static_pointer_cast<minifi::processors::TestFlowFileGenerator>(root->findProcessorByName("Generator"));
  auto sinkProc = std::static_pointer_cast<minifi::processors::TestProcessor>(root->findProcessorByName("TestProcessor"));

  std::promise<void> execSinkPromise;
  std::future<void> execSinkFuture = execSinkPromise.get_future();
  sinkProc->onTriggerCb_ = [&]{
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
  auto flowFilesEnqueued = [&] {return root->getTotalFlowFileCount() == 3;};
  REQUIRE(verifyWithBusyWait(std::chrono::milliseconds{200}, flowFilesEnqueued));

  REQUIRE(sourceProc->trigger_count.load() == 1);

  execSinkPromise.set_value();
  controller->stop();

  REQUIRE(sourceProc->trigger_count.load() == 1);
  REQUIRE(sinkProc->trigger_count.load() == 1);
}

TEST_CASE("Extend the waiting period during shutdown", "[TestFlow4]") {
  TestControllerWithFlow testController(yamlConfig);
  auto controller = testController.controller_;
  auto root = testController.root_;

  unsigned int timeout_ms = 1000;

  testController.configuration_->set(minifi::Configure::nifi_flowcontroller_drain_timeout, std::to_string(timeout_ms) + " ms");

  auto sourceProc = std::static_pointer_cast<minifi::processors::TestFlowFileGenerator>(root->findProcessorByName("Generator"));
  auto sinkProc = std::static_pointer_cast<minifi::processors::TestProcessor>(root->findProcessorByName("TestProcessor"));

  std::promise<void> execSinkPromise;
  std::future<void> execSinkFuture = execSinkPromise.get_future();
  sinkProc->onTriggerCb_ = [&]{
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
  auto flowFilesEnqueued = [&] {return root->getTotalFlowFileCount() == 3;};
  REQUIRE(verifyWithBusyWait(std::chrono::milliseconds{200}, flowFilesEnqueued));

  REQUIRE(sourceProc->trigger_count.load() == 1);

  std::thread shutdownThread([&]{
    execSinkPromise.set_value();
    controller->stop();
  });

  auto shutdownInitiated = std::chrono::steady_clock::now();
  auto shutdownDuration = [&] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - shutdownInitiated);};

  std::this_thread::sleep_for(std::chrono::milliseconds{500});
  while (shutdownDuration() < std::chrono::milliseconds(5000) && controller->isRunning()) {
    timeout_ms += 500;
    testController.getLogger()->log_info("Controller still running after %u ms, extending the waiting period to %u ms, ff count: %u",
        static_cast<unsigned int>(shutdownDuration().count()), timeout_ms, static_cast<unsigned int>(root->getTotalFlowFileCount()));
    testController.configuration_->set(minifi::Configure::nifi_flowcontroller_drain_timeout, std::to_string(timeout_ms) + " ms");
    std::this_thread::sleep_for(std::chrono::milliseconds{500});
  }

  REQUIRE(!controller->isRunning());
  REQUIRE(shutdownDuration().count() > 1500);

  shutdownThread.join();

  REQUIRE(sourceProc->trigger_count.load() == 1);
  REQUIRE(sinkProc->trigger_count.load() == 3);
}
