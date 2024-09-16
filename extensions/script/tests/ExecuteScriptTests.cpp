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
