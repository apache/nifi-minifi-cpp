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

#include "TestBase.h"
#include "Catch.h"
#include "AppendHostInfo.h"
#include "LogAttribute.h"

using AppendHostInfo = org::apache::nifi::minifi::processors::AppendHostInfo;
using LogAttribute = org::apache::nifi::minifi::processors::LogAttribute;

TEST_CASE("AppendHostInfoTest", "[appendhostinfotest]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  LogTestController::getInstance().setTrace<minifi::processors::AppendHostInfo>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
  std::shared_ptr<core::Processor> generate_flow_file = plan->addProcessor("GenerateFlowFile", "generate_flow_file");
  std::shared_ptr<core::Processor> append_host_info = plan->addProcessor("AppendHostInfo", "append_host_info", core::Relationship("success", "description"), true);
  std::shared_ptr<core::Processor> log_attribute = plan->addProcessor("LogAttribute", "log_attributes", core::Relationship("success", "description"), true);

  testController.runSession(plan);

  REQUIRE(LogTestController::getInstance().contains("source.hostname"));
  REQUIRE(LogTestController::getInstance().contains("source.ipv4"));
}

TEST_CASE("AppendHostInfoTestWithUnmatchableRegex", "[appendhostinfotestunmatchableregex]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  LogTestController::getInstance().setTrace<minifi::processors::AppendHostInfo>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
  std::shared_ptr<core::Processor> generate_flow_file = plan->addProcessor("GenerateFlowFile", "generate_flow_file");
  std::shared_ptr<core::Processor> append_host_info = plan->addProcessor("AppendHostInfo", "append_host_info", core::Relationship("success", "description"), true);
  std::shared_ptr<core::Processor> log_attribute = plan->addProcessor("LogAttribute", "log_attributes", core::Relationship("success", "description"), true);

  plan->setProperty(append_host_info, AppendHostInfo::InterfaceNameFilter, "\b");

  testController.runSession(plan);

  using namespace std::literals;
  REQUIRE(LogTestController::getInstance().contains("source.hostname", 0s, 0ms));
  REQUIRE_FALSE(LogTestController::getInstance().contains("source.ipv4", 0s, 0ms));
}

TEST_CASE("AppendHostInfoTestCanFilterOutLoopbackInterfacesWithRegex", "[appendhostinfotestfilterloopback]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  LogTestController::getInstance().setTrace<minifi::processors::AppendHostInfo>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
  std::shared_ptr<core::Processor> generate_flow_file = plan->addProcessor("GenerateFlowFile", "generate_flow_file");
  std::shared_ptr<core::Processor> append_host_info = plan->addProcessor("AppendHostInfo", "append_host_info", core::Relationship("success", "description"), true);
  std::shared_ptr<core::Processor> log_attribute = plan->addProcessor("LogAttribute", "log_attributes", core::Relationship("success", "description"), true);

  plan->setProperty(append_host_info, AppendHostInfo::InterfaceNameFilter, "(?!Loopback|lo).*?");  // set up the regex to accept everything except interfaces starting with Loopback or lo

  testController.runSession(plan);

  using namespace std::literals;
  REQUIRE(LogTestController::getInstance().contains("source.hostname", 0s, 0ms));
  REQUIRE_FALSE(LogTestController::getInstance().contains("127.0.0.1", 0s, 0ms));
}
