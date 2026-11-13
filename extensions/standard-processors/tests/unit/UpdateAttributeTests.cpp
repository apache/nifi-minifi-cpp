// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <memory>

#include "unit/TestBase.h"
#include "unit/Catch.h"

#include "LogAttribute.h"
#include "UpdateAttribute.h"
#include "GenerateFlowFile.h"

TEST_CASE("UpdateAttributeTest", "[updateAttributeTest]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::UpdateAttribute>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
  std::shared_ptr<TestPlan> plan = testController.createPlan();

  plan->addProcessor("GenerateFlowFile", "generate");
  const auto &update_proc = plan->addProcessor("UpdateAttribute", "update", core::Relationship("success", "description"), true);
  plan->addProcessor("LogAttribute", "log", core::Relationship("success", "description"), true);

  plan->setDynamicProperty(update_proc, "test_attr_1", "test_val_1");
  plan->setDynamicProperty(update_proc, "test_attr_2", "test_val_2");

  testController.runSession(plan, false);  // generate
  testController.runSession(plan, false);  // update
  testController.runSession(plan, false);  // log

  REQUIRE(LogTestController::getInstance().contains("key:test_attr_1 value:test_val_1"));
  REQUIRE(LogTestController::getInstance().contains("key:test_attr_2 value:test_val_2"));

  LogTestController::getInstance().reset();
}
