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
#include <set>

#include "SingleProcessorTestController.h"
#include "TestBase.h"
#include "Catch.h"

#include "ExecuteLuaScript.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "utils/file/FileUtils.h"
#include "utils/TestUtils.h"

namespace org::apache::nifi::minifi::extensions::lua::test {

TEST_CASE("Neither script body nor script file is set", "[executeLuaScriptMisconfiguration]") {
  TestController testController;
  auto plan = testController.createPlan();

  auto executeLuaScript = plan->addProcessor("ExecuteLuaScript", "executeLuaScript");


  REQUIRE_THROWS_AS(testController.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Test both script body and script file set", "[executeLuaScriptMisconfiguration]") {
  TestController testController;
  auto plan = testController.createPlan();

  auto executeLuaScript = plan->addProcessor("ExecuteLuaScript", "executeLuaScript");

  plan->setProperty(executeLuaScript, ExecuteLuaScript::ScriptFile.getName(), "/path/to/script.lua");
  plan->setProperty(executeLuaScript, ExecuteLuaScript::ScriptBody.getName(), R"(
    function onTrigger(context, session)
      log:info('hello from lua')
    end
  )");

  REQUIRE_THROWS_AS(testController.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Lua: Test Log", "[executeLuaScriptLuaLog]") {
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::LogAttribute>();
  logTestController.setDebug<ExecuteLuaScript>();

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto executeLuaScript = plan->addProcessor("ExecuteLuaScript",
                                          "executeLuaScript",
                                          core::Relationship("success", "description"),
                                          true);

  plan->setProperty(executeLuaScript, ExecuteLuaScript::ScriptBody.getName(), R"(
    function onTrigger(context, session)
      log:info('hello from lua')
    end
  )");

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir.string());

  utils::putFileToDir(getFileDir, "tstFile.ext", "tempFile");

  testController.runSession(plan, false);
  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains(
      "[org::apache::nifi::minifi::extensions::lua::ExecuteLuaScript] [info] hello from lua"));

  logTestController.reset();
}

TEST_CASE("Lua: Test Read File", "[executeLuaScriptLuaRead]") {
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::LogAttribute>();
  logTestController.setDebug<ExecuteLuaScript>();

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto logAttribute = plan->addProcessor("LogAttribute", "logAttribute",
                                         core::Relationship("success", "description"),
                                         true);
  auto executeLuaScript = plan->addProcessor("ExecuteLuaScript",
                                          "executeLuaScript",
                                          core::Relationship("success", "description"),
                                          true);
  auto putFile = plan->addProcessor("PutFile", "putFile", core::Relationship("success", "description"), true);

  plan->setProperty(executeLuaScript, ExecuteLuaScript::ScriptBody.getName(), R"(
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

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir.string());

  auto putFileDir = testController.createTempDirectory();
  plan->setProperty(putFile, minifi::processors::PutFile::Directory.getName(), putFileDir.string());

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  auto path = getFileDir / "tstFile.ext";
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();

  testController.runSession(plan, false);
  testController.runSession(plan, false);
  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  std::filesystem::remove(path);

  REQUIRE(logTestController.contains("[info] file content: tempFile"));

  // Verify that file content was preserved
  REQUIRE(!std::ifstream(path).good());
  auto moved_file = putFileDir / "tstFile.ext";
  REQUIRE(std::ifstream(moved_file).good());

  file.open(moved_file, std::ios::in);
  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  REQUIRE("tempFile" == contents);
  file.close();
  logTestController.reset();
}

TEST_CASE("Lua: Test Write File", "[executeLuaScriptLuaWrite]") {
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::LogAttribute>();
  logTestController.setDebug<ExecuteLuaScript>();

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto logAttribute = plan->addProcessor("LogAttribute", "logAttribute",
                                         core::Relationship("success", "description"),
                                         true);
  auto executeLuaScript = plan->addProcessor("ExecuteLuaScript",
                                          "executeLuaScript",
                                          core::Relationship("success", "description"),
                                          true);
  auto putFile = plan->addProcessor("PutFile", "putFile", core::Relationship("success", "description"), true);

  plan->setProperty(executeLuaScript, ExecuteLuaScript::ScriptBody.getName(), R"(
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

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir.string());

  auto putFileDir = testController.createTempDirectory();
  plan->setProperty(putFile, minifi::processors::PutFile::Directory.getName(), putFileDir.string());

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  auto path = getFileDir / "tstFile.ext";
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();

  testController.runSession(plan, false);
  testController.runSession(plan, false);
  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  std::filesystem::remove(path);

  // Verify new content was written
  REQUIRE(!std::ifstream(path).good());
  auto moved_file = putFileDir / "tstFile.ext";
  REQUIRE(std::ifstream(moved_file).good());

  file.open(moved_file, std::ios::in);
  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  REQUIRE("hello 2" == contents);
  file.close();
  logTestController.reset();
}

TEST_CASE("Lua: Test Update Attribute", "[executeLuaScriptLuaUpdateAttribute]") {
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::LogAttribute>();
  logTestController.setDebug<ExecuteLuaScript>();

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto executeLuaScript = plan->addProcessor("ExecuteLuaScript",
                                          "executeLuaScript",
                                          core::Relationship("success", "description"),
                                          true);
  auto logAttribute = plan->addProcessor("LogAttribute", "logAttribute",
                                         core::Relationship("success", "description"),
                                         true);

  plan->setProperty(executeLuaScript, ExecuteLuaScript::ScriptBody.getName(), R"(
    function onTrigger(context, session)
      flow_file = session:get()

      if flow_file ~= nil then
        log:info('got flow file: ' .. flow_file:getAttribute('filename'))
        flow_file:addAttribute('test_attr', '1')
        attr = flow_file:getAttribute('test_attr')
        log:info('got flow file attr \'test_attr\': ' .. attr)
        flow_file:updateAttribute('test_attr', tostring(attr + 1))
        session:transfer(flow_file, REL_SUCCESS)
      end
    end
  )");

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir.string());

  utils::putFileToDir(getFileDir, "tstFile.ext", "tempFile");

  REQUIRE_NOTHROW(testController.runSession(plan, false));
  REQUIRE_NOTHROW(testController.runSession(plan, false));
  REQUIRE_NOTHROW(testController.runSession(plan, false));

  REQUIRE(LogTestController::getInstance().contains("key:test_attr value:2"));

  logTestController.reset();
}

TEST_CASE("Lua: Test Create", "[executeLuaScriptLuaCreate]") {
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<ExecuteLuaScript>();

  auto plan = testController.createPlan();

  auto executeLuaScript = plan->addProcessor("ExecuteLuaScript",
                                          "executeLuaScript");

  plan->setProperty(executeLuaScript, ExecuteLuaScript::ScriptBody.getName(), R"(
    function onTrigger(context, session)
      flow_file = session:create(nil)

      if flow_file ~= nil then
        log:info('created flow file: ' .. flow_file:getAttribute('filename'))
        session:transfer(flow_file, REL_SUCCESS)
      end
    end
  )");

  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("[info] created flow file:"));

  logTestController.reset();
}

TEST_CASE("Lua: Test Require", "[executeLuaScriptLuaRequire]") {
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<ExecuteLuaScript>();

  auto plan = testController.createPlan();

  auto executeLuaScript = plan->addProcessor("ExecuteLuaScript",
                                          "executeLuaScript");

  plan->setProperty(executeLuaScript, ExecuteLuaScript::ScriptBody.getName(), R"(
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

TEST_CASE("Lua: Test Module Directory property", "[executeLuaScriptLuaModuleDirectoryProperty]") {
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<ExecuteLuaScript>();

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto executeLuaScript = plan->addProcessor("ExecuteLuaScript",
                                          "executeLuaScript",
                                          core::Relationship("success", "description"),
                                          true);

  const auto SCRIPT_FILES_DIRECTORY = std::filesystem::path(__FILE__).parent_path() / "test_lua_scripts";

  plan->setProperty(executeLuaScript, ExecuteLuaScript::ScriptFile.getName(), (SCRIPT_FILES_DIRECTORY / "foo_bar_processor.lua").string());
  plan->setProperty(executeLuaScript, ExecuteLuaScript::ModuleDirectory.getName(),
      (SCRIPT_FILES_DIRECTORY / "foo_modules" / "foo.lua").string() + "," + (SCRIPT_FILES_DIRECTORY / "bar_modules").string());

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir.string());

  utils::putFileToDir(getFileDir, "tstFile.ext", "tempFile");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("foobar"));

  logTestController.reset();
}

TEST_CASE("Lua: Non existent script file should throw", "[executeLuaScriptLuaNonExistentScriptFile]") {
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<ExecuteLuaScript>();

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto executeLuaScript = plan->addProcessor("ExecuteLuaScript",
                                          "executeLuaScript",
                                          core::Relationship("success", "description"),
                                          true);

  plan->setProperty(executeLuaScript, ExecuteLuaScript::ScriptFile.getName(), "/tmp/non-existent-file");

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir.string());

  utils::putFileToDir(getFileDir, "tstFile.ext", "tempFile");

  REQUIRE_THROWS_AS(testController.runSession(plan), minifi::Exception);

  logTestController.reset();
}

TEST_CASE("Lua can remove flowfiles", "[ExecuteLuaScript]") {
  const auto execute_script = std::make_shared<ExecuteLuaScript>("ExecuteLuaScript");

  minifi::test::SingleProcessorTestController controller{execute_script};
  LogTestController::getInstance().setTrace<ExecuteLuaScript>();
  execute_script->setProperty(ExecuteLuaScript::ScriptBody.getName(),
      R"(
        function onTrigger(context, session)
          flow_file = session:get()
          session:remove(flow_file)
        end
      )");
  REQUIRE_NOTHROW(controller.trigger("hello"));
}

}  // namespace org::apache::nifi::minifi::extensions::lua::test
