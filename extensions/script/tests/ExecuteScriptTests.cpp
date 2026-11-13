// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#define EXTENSION_LIST "*minifi-*,!*python*,!*lua*"  // NOLINT(cppcoreguidelines-macro-usage)

#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

#include "../ExecuteScript.h"


namespace org::apache::nifi::minifi::processors::test {

TEST_CASE("Script engine is not set", "[executescriptMisconfiguration]") {
  TestController testController;
  auto plan = testController.createPlan();

  auto executeScript = plan->addProcessor("ExecuteScript", "executeScript");

  CHECK_FALSE(plan->setProperty(executeScript, ExecuteScript::ScriptEngine, ""));
  CHECK(plan->setProperty(executeScript, ExecuteScript::ScriptFile, "/path/to/script"));
}

TEST_CASE("Script engine is not available", "[executescriptMisconfiguration]") {
  TestController testController;
  auto plan = testController.createPlan();

  auto executeScript = plan->addProcessor("ExecuteScript", "executeScript");

  SECTION("lua") {
    plan->setProperty(executeScript, ExecuteScript::ScriptEngine, "lua");
    REQUIRE_THROWS_WITH(testController.runSession(plan, true), "Process Schedule Operation: Could not instantiate: LuaScriptExecutor. Make sure that the lua scripting extension is loaded");
  }
  SECTION("python") {
    plan->setProperty(executeScript, ExecuteScript::ScriptEngine, "python");
    REQUIRE_THROWS_WITH(testController.runSession(plan, true), "Process Schedule Operation: Could not instantiate: PythonScriptExecutor. Make sure that the python scripting extension is loaded");
  }
}


}  // namespace org::apache::nifi::minifi::processors::test
