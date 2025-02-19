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

#include <filesystem>
#include <memory>
#include <string>

#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

#include "../../script/ExecuteScript.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"

namespace org::apache::nifi::minifi::processors::test {

TEST_CASE("Lua: hello world") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "lua");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(print("Hello world!"))");

  CHECK_NOTHROW(controller.trigger());
}

TEST_CASE("Script engine is not set", "[executescriptMisconfiguration]") {
  TestController test_controller;
  auto plan = test_controller.createPlan();

  auto execute_script = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(execute_script, ExecuteScript::ScriptEngine, "");
  plan->setProperty(execute_script, ExecuteScript::ScriptFile, "/path/to/script.lua");

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Neither script body nor script file is set", "[executescriptMisconfiguration]") {
  TestController test_controller;
  auto plan = test_controller.createPlan();

  auto execute_script = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(execute_script, ExecuteScript::ScriptEngine, "lua");

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Test both script body and script file set", "[executescriptMisconfiguration]") {
  TestController test_controller;
  auto plan = test_controller.createPlan();

  auto execute_script = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(execute_script, ExecuteScript::ScriptEngine, "lua");
  plan->setProperty(execute_script, ExecuteScript::ScriptFile, "/path/to/script.lua");
  plan->setProperty(execute_script, ExecuteScript::ScriptBody, R"(
function onTrigger(context, session)
  log:info('hello from lua')
end
  )");

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Lua: Test session get should return None if there are no flowfiles in the incoming connections") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "lua");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
function onTrigger(context, session)
  flow_file = session:get()

  if flow_file ~= nil then
    error("Didn't expect flow_file")
  end
end
  )");
  auto result = controller.trigger();
  REQUIRE(result.at(ExecuteScript::Success).empty());
  REQUIRE(result.at(ExecuteScript::Failure).empty());
}


TEST_CASE("Lua: Test Log", "[executescriptLuaLog]") {
  LogTestController::getInstance().reset();
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "lua");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
function onTrigger(context, session)
  log:info('hello from lua')
end
  )");

  auto result = controller.trigger();
  REQUIRE(result.at(ExecuteScript::Success).empty());
  REQUIRE(result.at(ExecuteScript::Failure).empty());

  REQUIRE(LogTestController::getInstance().contains("[org::apache::nifi::minifi::processors::ExecuteScript] [info] hello from lua"));
}

TEST_CASE("Lua: Test Read File", "[executescriptLuaRead]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "lua");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
read_callback = {}

function read_callback.process(self, input_stream)
    content = input_stream:read(0)
    log:info('file content: ' .. content)
    return #content
end

function onTrigger(context, session)
  flow_file = session:get()

  if flow_file ~= nil then
    log:info('got flow file: ' .. flow_file:getAttribute('filename'))
    session:read(flow_file, read_callback)
    session:transfer(flow_file, REL_SUCCESS)
  end
end
  )");

  auto result = controller.trigger("tempFile");
  REQUIRE(result.at(ExecuteScript::Success).size() == 1);
  CHECK(controller.plan->getContent(result.at(ExecuteScript::Success)[0]) == "tempFile");
}

TEST_CASE("Lua: Test Write File", "[executescriptLuaWrite]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "lua");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
    write_callback = {}

    function write_callback.process(self, output_stream)
      new_content = 'hello 2'
      output_stream:write(new_content)
      return #new_content
    end

    function onTrigger(context, session)
      flow_file = session:get()

      if flow_file ~= nil then
        log:info('got flow file: ' .. flow_file:getAttribute('filename'))
        session:write(flow_file, write_callback)
        session:transfer(flow_file, REL_SUCCESS)
      end
    end
  )");


  auto result = controller.trigger("tempFile");
  REQUIRE(result.at(ExecuteScript::Success).size() == 1);
  CHECK(controller.plan->getContent(result.at(ExecuteScript::Success)[0]) == "hello 2");
}

TEST_CASE("Lua: Test Create", "[executescriptLuaCreate]") {
  LogTestController::getInstance().reset();
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "lua");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
function onTrigger(context, session)
  flow_file = session:create(nil)

  if flow_file ~= nil then
    log:info('created flow file: ' .. flow_file:getAttribute('filename'))
    session:transfer(flow_file, REL_SUCCESS)
  end
end
  )");


  auto result = controller.trigger();
  REQUIRE(result.at(ExecuteScript::Success).size() == 1);
  REQUIRE(result.at(ExecuteScript::Failure).empty());
  REQUIRE(LogTestController::getInstance().contains("[info] created flow file:"));
}

TEST_CASE("Lua: Test Update Attribute", "[executescriptLuaUpdateAttribute]") {
  LogTestController::getInstance().reset();
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "lua");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
function onTrigger(context, session)
  flow_file = session:get()

  if flow_file ~= nil then
    log:info('got flow file: ' .. flow_file:getAttribute('filename'))
    flow_file:addAttribute('test_attr', '1')
    attr = tonumber(flow_file:getAttribute('test_attr'))
    log:info('got flow file attr \'test_attr\': ' .. tostring(attr))
    flow_file:updateAttribute('test_attr', tostring(attr + 1))
    session:transfer(flow_file, REL_SUCCESS)
  end
end
  )");

  auto result = controller.trigger("tempFile");
  REQUIRE(result.at(ExecuteScript::Success).size() == 1);
  CHECK(controller.plan->getContent(result.at(ExecuteScript::Success)[0]) == "tempFile");
  CHECK(result.at(ExecuteScript::Success)[0]->getAttribute("test_attr") == "2");
}

TEST_CASE("Lua: Test Require", "[executescriptLuaRequire]") {
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<ExecuteScript>();

  auto plan = testController.createPlan();

  auto executeScript = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(executeScript, ExecuteScript::ScriptEngine, "lua");
  plan->setProperty(executeScript, ExecuteScript::ScriptBody, R"(
    require 'os'
    require 'coroutine'
    require 'math'
    require 'io'
    require 'string'
    require 'table'
    require 'package'

    log:info('OK')

    function onTrigger(context, session)
    end
  )");

  REQUIRE_NOTHROW(testController.runSession(plan, false));

  REQUIRE(LogTestController::getInstance().contains("[info] OK"));

  logTestController.reset();
}

TEST_CASE("Lua: Test Module Directory property", "[executescriptLuaModuleDirectoryProperty]") {
  using org::apache::nifi::minifi::utils::file::get_executable_dir;

  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  const auto script_files_directory =  minifi::utils::file::FileUtils::get_executable_dir() / "resources" / "test_lua_scripts";

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "lua");
  execute_script->setProperty(ExecuteScript::ScriptFile.name, (script_files_directory / "foo_bar_processor.lua").string());
  execute_script->setProperty(ExecuteScript::ModuleDirectory.name, (script_files_directory / "foo_modules" / "foo.lua").string() + "," + (script_files_directory / "bar_modules").string());

  auto result = controller.trigger("tempFile");
  REQUIRE(result.at(ExecuteScript::Success).size() == 1);
  REQUIRE(result.at(ExecuteScript::Failure).empty());

  REQUIRE(LogTestController::getInstance().contains("foobar"));
}

TEST_CASE("Lua: Non existent script file should throw", "[executescriptLuaNonExistentScriptFile]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "lua");
  execute_script->setProperty(ExecuteScript::ScriptFile.name, "/tmp/non-existent-file");

  REQUIRE_THROWS_AS(controller.trigger("tempFile"), minifi::Exception);
}

TEST_CASE("Lua can remove flowfiles", "[ExecuteScript]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();
  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "lua");
  execute_script->setProperty(ExecuteScript::ScriptBody.name,
      R"(
        function onTrigger(context, session)
          flow_file = session:get()
          session:remove(flow_file)
        end
      )");
  auto result = controller.trigger("hello");
  REQUIRE(result.at(ExecuteScript::Success).empty());
  REQUIRE(result.at(ExecuteScript::Failure).empty());
}

TEST_CASE("Lua can store states in StateManager", "[ExecuteScript]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<minifi::processors::ExecuteScript>();
  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "lua");
  execute_script->setProperty(ExecuteScript::ScriptBody.name,
      R"(
        function onTrigger(context, session)
          state_manager = context:getStateManager()
          state = state_manager:get()
          if state == nil then
            state = {}
            state['lua_trigger_count'] = 0
          end
          lua_trigger_count = state['lua_trigger_count']
          log:info('lua_trigger_count: ' .. lua_trigger_count)
          state['lua_trigger_count'] = tostring(tonumber(lua_trigger_count) + 1)
          state_manager:set(state)
        end
      )");

  for (size_t i = 0; i < 4; ++i) {
    controller.trigger();
    CHECK(LogTestController::getInstance().contains(fmt::format("lua_trigger_count: {}", i)));
  }
}

}  // namespace org::apache::nifi::minifi::processors::test
