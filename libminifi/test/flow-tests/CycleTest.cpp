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
#include "CustomProcessors.h"
#include "unit/TestControllerWithFlow.h"

// A flow with structure:
//
// [Generator] ----> [A] -----> [B]
//                    ^__________|
const char* flowConfigurationYaml =
    R"(
Flow Controller:
  name: MiNiFi Flow
  id: 2438e3c8-015a-1001-79ca-83af40ec1990
Processors:
  - name: Generator
    id: 2438e3c8-015a-1001-79ca-83af40ec1991
    class: org.apache.nifi.processors.TestFlowFileGenerator
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
  - name: A
    id: 2438e3c8-015a-1001-79ca-83af40ec1992
    class: org.apache.nifi.processors.TestProcessor
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      AppleProbability: 100
      BananaProbability: 0
  - name: B
    id: 2438e3c8-015a-1001-79ca-83af40ec2000
    class: org.apache.nifi.processors.TestProcessor
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      AppleProbability: 100
      BananaProbability: 0

Connections:
  - name: Gen
    id: 2438e3c8-015a-1001-79ca-83af40ec1993
    source name: Generator
    destination name: A
    source relationship name: success
    max work queue size: 1
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: Forward
    id: 2438e3c8-015a-1001-79ca-83af40ec1994
    source name: A
    destination name: B
    source relationship name: apple
    max work queue size: 1
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: Backward
    id: 2438e3c8-015a-1001-79ca-83af40ec1995
    source name: B
    destination name: A
    source relationship name: apple
    max work queue size: 1
    max work queue data size: 1 MB
    flowfile expiration: 0

Remote Processing Groups:

Controller Services:
  - name: defaultstatestorage
    id: 2438e3c8-015a-1000-79ca-83af40ec1994
    class: PersistentMapStateStorage
    Properties:
      Auto Persistence Interval:
          - value: 0 sec
      File:
          - value: cycletest_state.txt
)";

TEST_CASE("Flow with two long cycle", "[FlowWithCycle]") {
  TestControllerWithFlow testController(flowConfigurationYaml);
  testController.startFlow();

  auto controller = testController.controller_;
  auto root = testController.root_;

  auto procGenerator = dynamic_cast<org::apache::nifi::minifi::processors::TestFlowFileGenerator*>(root->findProcessorByName("Generator"));
  auto procA = dynamic_cast<org::apache::nifi::minifi::processors::TestProcessor*>(root->findProcessorByName("A"));
  auto procB = dynamic_cast<org::apache::nifi::minifi::processors::TestProcessor*>(root->findProcessorByName("B"));

  int tryCount = 0;
  while (tryCount++ < 10 && (procA->trigger_count.load() < 10 || procB->trigger_count.load() < 10)) {
    std::this_thread::sleep_for(std::chrono::milliseconds{1000});
  }

  REQUIRE(procGenerator->trigger_count.load() <= 3);
  REQUIRE(procA->trigger_count.load() >= 10);
  REQUIRE(procB->trigger_count.load() >= 10);
}
