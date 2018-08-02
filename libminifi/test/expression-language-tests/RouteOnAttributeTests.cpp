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
#include "../TestBase.h"
#include <RouteOnAttribute.h>
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "processors/GenerateFlowFile.h"

TEST_CASE("RouteOnAttributeMatchedTest", "[routeOnAttributeMatchedTest]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::UpdateAttribute>();
  LogTestController::getInstance().setDebug<minifi::processors::RouteOnAttribute>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  const auto &generate_proc = plan->addProcessor("GenerateFlowFile", "generate");

  const auto &update_proc = plan->addProcessor("UpdateAttribute", "update", core::Relationship("success", "description"), true);
  plan->setProperty(update_proc, "route_condition_attr", "true", true);

  const auto &route_proc = plan->addProcessor("RouteOnAttribute", "route", core::Relationship("success", "description"), true);
  route_proc->setAutoTerminatedRelationships({ { core::Relationship("unmatched", "description") } });
  plan->setProperty(route_proc, "route_matched", "${route_condition_attr}", true);

  const auto &update_matched_proc = plan->addProcessor("UpdateAttribute", "update_matched", core::Relationship("route_matched", "description"), true);
  plan->setProperty(update_matched_proc, "route_check_attr", "good", true);

  const auto &log_proc = plan->addProcessor("LogAttribute", "log", core::Relationship("success", "description"), true);

  testController.runSession(plan, false);  // generate
  testController.runSession(plan, false);  // update
  testController.runSession(plan, false);  // route
  testController.runSession(plan, false);  // update_matched
  testController.runSession(plan, false);  // log

  REQUIRE(LogTestController::getInstance().contains("key:route_check_attr value:good"));

  LogTestController::getInstance().reset();
}

TEST_CASE("RouteOnAttributeUnmatchedTest", "[routeOnAttributeUnmatchedTest]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::UpdateAttribute>();
  LogTestController::getInstance().setDebug<minifi::processors::RouteOnAttribute>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  const auto &generate_proc = plan->addProcessor("GenerateFlowFile", "generate");

  const auto &update_proc = plan->addProcessor("UpdateAttribute", "update", core::Relationship("success", "description"), true);
  plan->setProperty(update_proc, "route_condition_attr", "false", true);

  const auto &route_proc = plan->addProcessor("RouteOnAttribute", "route", core::Relationship("success", "description"), true);
  plan->setProperty(route_proc, "route_matched", "${route_condition_attr}", true);

  const auto &update_matched_proc = plan->addProcessor("UpdateAttribute", "update_matched", core::Relationship("unmatched", "description"), true);
  plan->setProperty(update_matched_proc, "route_check_attr", "good", true);

  const auto &log_proc = plan->addProcessor("LogAttribute", "log", core::Relationship("success", "description"), true);

  testController.runSession(plan, false);  // generate
  testController.runSession(plan, false);  // update
  testController.runSession(plan, false);  // route
  testController.runSession(plan, false);  // update_matched
  testController.runSession(plan, false);  // log

  REQUIRE(LogTestController::getInstance().contains("key:route_check_attr value:good"));

  LogTestController::getInstance().reset();
}
