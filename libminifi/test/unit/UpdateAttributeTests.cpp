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
#include "../TestBase.h"
#include <memory>
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "processors/GenerateFlowFile.h"

TEST_CASE("UpdateAttributeTest", "[updateAttributeTest]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::UpdateAttribute>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
  std::shared_ptr<TestPlan> plan = testController.createPlan();

  const auto &generate_proc = plan->addProcessor("GenerateFlowFile", "generate");
  const auto &update_proc = plan->addProcessor("UpdateAttribute", "update", core::Relationship("success", "description"), true);
  const auto &log_proc = plan->addProcessor("LogAttribute", "log", core::Relationship("success", "description"), true);

  plan->setProperty(update_proc, "test_attr_1", "test_val_1", true);
  plan->setProperty(update_proc, "test_attr_2", "test_val_2", true);

  testController.runSession(plan, false);  // generate
  testController.runSession(plan, false);  // update
  testController.runSession(plan, false);  // log

  REQUIRE(LogTestController::getInstance().contains("key:test_attr_1 value:test_val_1"));
  REQUIRE(LogTestController::getInstance().contains("key:test_attr_2 value:test_val_2"));

  LogTestController::getInstance().reset();
}
