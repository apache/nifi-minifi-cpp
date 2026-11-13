// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <array>
#include <memory>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include <RouteOnAttribute.h>
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"

TEST_CASE("RouteOnAttributeMatchedTest", "[routeOnAttributeMatchedTest]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::UpdateAttribute>();
  LogTestController::getInstance().setDebug<minifi::processors::RouteOnAttribute>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  plan->addProcessor("GenerateFlowFile", "generate");

  const auto &update_proc = plan->addProcessor("UpdateAttribute", "update", core::Relationship("success", "description"), true);
  plan->setDynamicProperty(update_proc, "route_condition_attr", "true");

  const auto &route_proc = plan->addProcessor("RouteOnAttribute", "route", core::Relationship("success", "description"), true);
  route_proc->setAutoTerminatedRelationships(std::array{core::Relationship("unmatched", "description")});
  plan->setDynamicProperty(route_proc, "route_matched", "${route_condition_attr}");

  const auto &update_matched_proc = plan->addProcessor("UpdateAttribute", "update_matched", core::Relationship("route_matched", "description"), true);
  plan->setDynamicProperty(update_matched_proc, "route_check_attr", "good");

  plan->addProcessor("LogAttribute", "log", core::Relationship("success", "description"), true);

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

  plan->addProcessor("GenerateFlowFile", "generate");

  const auto &update_proc = plan->addProcessor("UpdateAttribute", "update", core::Relationship("success", "description"), true);
  plan->setDynamicProperty(update_proc, "route_condition_attr", "false");

  const auto &route_proc = plan->addProcessor("RouteOnAttribute", "route", core::Relationship("success", "description"), true);
  plan->setDynamicProperty(route_proc, "route_matched", "${route_condition_attr}");

  const auto &update_matched_proc = plan->addProcessor("UpdateAttribute", "update_matched", core::Relationship("unmatched", "description"), true);
  plan->setDynamicProperty(update_matched_proc, "route_check_attr", "good");

  plan->addProcessor("LogAttribute", "log", core::Relationship("success", "description"), true);

  testController.runSession(plan, false);  // generate
  testController.runSession(plan, false);  // update
  testController.runSession(plan, false);  // route
  testController.runSession(plan, false);  // update_matched
  testController.runSession(plan, false);  // log

  REQUIRE(LogTestController::getInstance().contains("key:route_check_attr value:good"));

  LogTestController::getInstance().reset();
}
