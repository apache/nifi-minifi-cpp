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
#include "AppendHostInfo.h"
#include "LogAttribute.h"

using AppendHostInfo = org::apache::nifi::minifi::processors::AppendHostInfo;
using LogAttribute = org::apache::nifi::minifi::processors::LogAttribute;

TEST_CASE("AppendHostInfoTest", "[appendhostinfotest]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  LogTestController::getInstance().setTrace<processors::AppendHostInfo>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  std::shared_ptr<core::Processor> generate_flow_file = plan->addProcessor("GenerateFlowFile", "generate_flow_file");
  std::shared_ptr<core::Processor> append_host_info = plan->addProcessor("AppendHostInfo", "append_host_info", core::Relationship("success", "description"), true);
  std::shared_ptr<core::Processor> log_attribute = plan->addProcessor("LogAttribute", "log_attributes", core::Relationship("success", "description"), true);

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // AppendHostInfo
  plan->runNextProcessor();  // LogAttribute

  REQUIRE(LogTestController::getInstance().contains("source.hostname"));
  REQUIRE(LogTestController::getInstance().contains("source.ipv4"));
}

TEST_CASE("AppendHostInfoTestWithUnmatchableRegex", "[appendhostinfotestunmatchableregex]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  LogTestController::getInstance().setTrace<processors::AppendHostInfo>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  std::shared_ptr<core::Processor> generate_flow_file = plan->addProcessor("GenerateFlowFile", "generate_flow_file");
  std::shared_ptr<core::Processor> append_host_info = plan->addProcessor("AppendHostInfo", "append_host_info", core::Relationship("success", "description"), true);
  std::shared_ptr<core::Processor> log_attribute = plan->addProcessor("LogAttribute", "log_attributes", core::Relationship("success", "description"), true);

  plan->setProperty(append_host_info, AppendHostInfo::InterfaceNameFilter.getName(), "^\b$");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // AppendHostInfo
  plan->runNextProcessor();  // LogAttribute

  REQUIRE(LogTestController::getInstance().contains("source.hostname", std::chrono::seconds(0), std::chrono::milliseconds(0)));
  REQUIRE_FALSE(LogTestController::getInstance().contains("source.ipv4", std::chrono::seconds(0), std::chrono::milliseconds(0)));
}

TEST_CASE("AppendHostInfoTestFilterLoopback", "[appendhostinfotestfilterloopback]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  LogTestController::getInstance().setTrace<processors::AppendHostInfo>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  std::shared_ptr<core::Processor> generate_flow_file = plan->addProcessor("GenerateFlowFile", "generate_flow_file");
  std::shared_ptr<core::Processor> append_host_info = plan->addProcessor("AppendHostInfo", "append_host_info", core::Relationship("success", "description"), true);
  std::shared_ptr<core::Processor> log_attribute = plan->addProcessor("LogAttribute", "log_attributes", core::Relationship("success", "description"), true);

  plan->setProperty(append_host_info, AppendHostInfo::InterfaceNameFilter.getName(), "^((?!Loopback|lo).)*$");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // AppendHostInfo
  plan->runNextProcessor();  // LogAttribute

  REQUIRE(LogTestController::getInstance().contains("source.hostname", std::chrono::seconds(0), std::chrono::milliseconds(0)));
  REQUIRE_FALSE(LogTestController::getInstance().contains("127.0.0.1", std::chrono::seconds(0), std::chrono::milliseconds(0)));
}
