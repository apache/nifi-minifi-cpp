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

#define CATCH_CONFIG_MAIN

#include <memory>
#include <string>
#include <set>

#include "../TestBase.h"

#include <ExecuteScript.h>
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"

TEST_CASE("Lua: Test Log", "[executescriptLuaLog]") { // NOLINT
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::LogAttribute>();
  logTestController.setDebug<minifi::processors::ExecuteScript>();

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);

  plan->setProperty(executeScript, processors::ExecuteScript::ScriptEngine.getName(), "lua");
  plan->setProperty(executeScript, processors::ExecuteScript::ScriptBody.getName(), R"(
    function onTrigger(context, session)
      log:info('hello from lua')
    end
  )");

  char getFileDirFmt[] = "/tmp/ft.XXXXXX";
  char *getFileDir = testController.createTempDirectory(getFileDirFmt);
  plan->setProperty(getFile, processors::GetFile::Directory.getName(), getFileDir);

  std::fstream file;
  std::stringstream ss;
  ss << getFileDir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();

  testController.runSession(plan, false);
  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains(
      "[org::apache::nifi::minifi::processors::ExecuteScript] [info] hello from lua"));

  logTestController.reset();
}

TEST_CASE("Lua: Test Read File", "[executescriptLuaRead]") { // NOLINT
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::LogAttribute>();
  logTestController.setDebug<minifi::processors::ExecuteScript>();

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto logAttribute = plan->addProcessor("LogAttribute", "logAttribute",
                                         core::Relationship("success", "description"),
                                         true);
  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);
  auto putFile = plan->addProcessor("PutFile", "putFile", core::Relationship("success", "description"), true);

  plan->setProperty(executeScript, processors::ExecuteScript::ScriptEngine.getName(), "lua");
  plan->setProperty(executeScript, processors::ExecuteScript::ScriptBody.getName(), R"(
    read_callback = {}

    function read_callback.process(self, input_stream)
        content = input_stream:read()
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

  char getFileDirFmt[] = "/tmp/ft.XXXXXX";
  char *getFileDir = testController.createTempDirectory(getFileDirFmt);
  plan->setProperty(getFile, processors::GetFile::Directory.getName(), getFileDir);

  char putFileDirFmt[] = "/tmp/ft.XXXXXX";
  char *putFileDir = testController.createTempDirectory(putFileDirFmt);
  plan->setProperty(putFile, processors::PutFile::Directory.getName(), putFileDir);

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  std::stringstream ss;
  ss << getFileDir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();

  testController.runSession(plan, false);
  testController.runSession(plan, false);
  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  unlink(ss.str().c_str());

  REQUIRE(logTestController.contains("[info] file content: tempFile"));

  // Verify that file content was preserved
  REQUIRE(!std::ifstream(ss.str()).good());
  std::stringstream movedFile;
  movedFile << putFileDir << "/" << "tstFile.ext";
  REQUIRE(std::ifstream(movedFile.str()).good());

  file.open(movedFile.str(), std::ios::in);
  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  REQUIRE("tempFile" == contents);
  file.close();
  logTestController.reset();
}

TEST_CASE("Lua: Test Write File", "[executescriptLuaWrite]") { // NOLINT
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::LogAttribute>();
  logTestController.setDebug<minifi::processors::ExecuteScript>();

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto logAttribute = plan->addProcessor("LogAttribute", "logAttribute",
                                         core::Relationship("success", "description"),
                                         true);
  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);
  auto putFile = plan->addProcessor("PutFile", "putFile", core::Relationship("success", "description"), true);

  plan->setProperty(executeScript, processors::ExecuteScript::ScriptEngine.getName(), "lua");
  plan->setProperty(executeScript, processors::ExecuteScript::ScriptBody.getName(), R"(
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

  char getFileDirFmt[] = "/tmp/ft.XXXXXX";
  char *getFileDir = testController.createTempDirectory(getFileDirFmt);
  plan->setProperty(getFile, processors::GetFile::Directory.getName(), getFileDir);

  char putFileDirFmt[] = "/tmp/ft.XXXXXX";
  char *putFileDir = testController.createTempDirectory(putFileDirFmt);
  plan->setProperty(putFile, processors::PutFile::Directory.getName(), putFileDir);

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  std::stringstream ss;
  ss << getFileDir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();

  testController.runSession(plan, false);
  testController.runSession(plan, false);
  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  unlink(ss.str().c_str());

  // Verify new content was written
  REQUIRE(!std::ifstream(ss.str()).good());
  std::stringstream movedFile;
  movedFile << putFileDir << "/" << "tstFile.ext";
  REQUIRE(std::ifstream(movedFile.str()).good());

  file.open(movedFile.str(), std::ios::in);
  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  REQUIRE("hello 2" == contents);
  file.close();
  logTestController.reset();
}

TEST_CASE("Lua: Test Update Attribute", "[executescriptLuaUpdateAttribute]") { // NOLINT
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::LogAttribute>();
  logTestController.setDebug<minifi::processors::ExecuteScript>();

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);
  auto logAttribute = plan->addProcessor("LogAttribute", "logAttribute",
                                         core::Relationship("success", "description"),
                                         true);

  plan->setProperty(executeScript, processors::ExecuteScript::ScriptEngine.getName(), "lua");
  plan->setProperty(executeScript, processors::ExecuteScript::ScriptBody.getName(), R"(
    function onTrigger(context, session)
      flow_file = session:get()

      if flow_file ~= nil then
        log:info('got flow file: ' .. flow_file:getAttribute('filename'))
        flow_file:addAttribute('test_attr', '1')
        attr = flow_file:getAttribute('test_attr')
        log:info('got flow file attr \'test_attr\': ' .. attr)
        flow_file:updateAttribute('test_attr', attr + 1)
        session:transfer(flow_file, REL_SUCCESS)
      end
    end
  )");

  char getFileDirFmt[] = "/tmp/ft.XXXXXX";
  char *getFileDir = testController.createTempDirectory(getFileDirFmt);
  plan->setProperty(getFile, processors::GetFile::Directory.getName(), getFileDir);

  std::fstream file;
  std::stringstream ss;
  ss << getFileDir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();

  testController.runSession(plan, false);
  testController.runSession(plan, false);
  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("key:test_attr value:2"));

  logTestController.reset();
}

TEST_CASE("Lua: Test Create", "[executescriptLuaCreate]") { // NOLINT
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::ExecuteScript>();

  auto plan = testController.createPlan();

  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript");

  plan->setProperty(executeScript, processors::ExecuteScript::ScriptEngine.getName(), "lua");
  plan->setProperty(executeScript, processors::ExecuteScript::ScriptBody.getName(), R"(
    function onTrigger(context, session)
      flow_file = session:create(nil)

      if flow_file ~= nil then
        log:info('created flow file: ' .. flow_file:getAttribute('filename'))
        session:transfer(flow_file, REL_SUCCESS)
      end
    end
  )");

  plan->reset();

  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("[info] created flow file:"));

  logTestController.reset();
}

TEST_CASE("Lua: Test Require", "[executescriptLuaRequire]") { // NOLINT
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::ExecuteScript>();

  auto plan = testController.createPlan();

  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript");

  plan->setProperty(executeScript, processors::ExecuteScript::ScriptEngine.getName(), "lua");
  plan->setProperty(executeScript, processors::ExecuteScript::ScriptBody.getName(), R"(
    require 'os'
    require 'coroutine'
    require 'math'
    require 'io'
    require 'string'
    require 'table'
    require 'package'

    log:info('OK')
  )");

  plan->reset();

  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("[info] OK"));

  logTestController.reset();
}
