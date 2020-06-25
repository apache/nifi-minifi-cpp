/**
 * @file LoopTest.cpp
 * ProcessSession class implementation
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
#include "FlowBuilder.h"

// A flow with structure:
// [Generator] ---> [A] ---|
//                   ^_____|
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

Connections:
  - name: Gen
    id: 2438e3c8-015a-1001-79ca-83af40ec1993
    source name: Generator
    source relationship name: success
    destination name: A
    max work queue size: 1
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: Loop
    id: 2438e3c8-015a-1001-79ca-83af40ec1994
    source name: A
    destination name: A
    source relationship name: apple
    max work queue size: 1
    max work queue data size: 1 MB
    flowfile expiration: 0

Remote Processing Groups:
)";

TEST_CASE("Flow with a single loop", "[SingleLoopFlow]") {
  TestController testController;

  LogTestController::getInstance().setTrace<core::Processor>();
  LogTestController::getInstance().setTrace<minifi::Connection>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();

  char format[] = "/tmp/flow.XXXXXX";
  std::string dir = testController.createTempDirectory(format);
  std::string yamlPath = utils::file::FileUtils::concat_path(dir, "config.yml");
  std::ofstream{yamlPath} << flowConfigurationYaml;

  Flow flow = createFlow(yamlPath);

  std::this_thread::sleep_for(std::chrono::milliseconds{2000});

  auto procGenerator = std::static_pointer_cast<org::apache::nifi::minifi::processors::TestFlowFileGenerator>(flow.root_->findProcessor("Generator"));
  auto procA = std::static_pointer_cast<org::apache::nifi::minifi::processors::TestProcessor>(flow.root_->findProcessor("A"));

  REQUIRE(procGenerator->trigger_count.load() <= 2);
  REQUIRE(procA->trigger_count.load() > 15);
}
