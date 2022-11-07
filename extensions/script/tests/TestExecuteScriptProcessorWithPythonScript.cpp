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
#include <string>
#include <set>

#include "TestBase.h"
#include "Catch.h"

#include <ExecuteScript.h>
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "utils/TestUtils.h"

TEST_CASE("Script engine is not set", "[executescriptMisconfiguration]") {
  TestController testController;
  auto plan = testController.createPlan();

  auto executeScript = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptEngine.getName(), "");
  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptFile.getName(), "/path/to/script.py");

  REQUIRE_THROWS_AS(testController.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Neither script body nor script file is set", "[executescriptMisconfiguration]") {
  TestController testController;
  auto plan = testController.createPlan();

  auto executeScript = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptEngine.getName(), "python");

  REQUIRE_THROWS_AS(testController.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Test both script body and script file set", "[executescriptMisconfiguration]") {
  TestController testController;
  auto plan = testController.createPlan();

  auto executeScript = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptEngine.getName(), "python");
  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptFile.getName(), "/path/to/script.py");
  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptBody.getName(), R"(
    def onTrigger(context, session):
      log.info('hello from python')
  )");

  REQUIRE_THROWS_AS(testController.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Python: Test Read File", "[executescriptPythonRead]") {
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

  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptBody.getName(), R"(
    import codecs

    class ReadCallback(object):
      def process(self, input_stream):
        content = codecs.getreader('utf-8')(input_stream).read()
        log.info('file content: %s' % content)
        return len(content)

    def onTrigger(context, session):
      flow_file = session.get()

      if flow_file is not None:
        log.info('got flow file: %s' % flow_file.getAttribute('filename'))
        session.read(flow_file, ReadCallback())
        session.transfer(flow_file, REL_SUCCESS)
  )");

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir);

  auto putFileDir = testController.createTempDirectory();
  plan->setProperty(putFile, minifi::processors::PutFile::Directory.getName(), putFileDir);

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

TEST_CASE("Python: Test Write File", "[executescriptPythonWrite]") {
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

  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptBody.getName(), R"(
    class WriteCallback(object):
      def process(self, output_stream):
        new_content = 'hello 2'.encode('utf-8')
        output_stream.write(new_content)
        return len(new_content)

    def onTrigger(context, session):
      flow_file = session.get()
      if flow_file is not None:
        log.info('got flow file: %s' % flow_file.getAttribute('filename'))
        session.write(flow_file, WriteCallback())
        session.transfer(flow_file, REL_SUCCESS)
  )");

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir);

  auto putFileDir = testController.createTempDirectory();
  plan->setProperty(putFile, minifi::processors::PutFile::Directory.getName(), putFileDir);

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

TEST_CASE("Python: Test Create", "[executescriptPythonCreate]") {
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::ExecuteScript>();

  auto plan = testController.createPlan();

  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript");

  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptBody.getName(), R"(
    def onTrigger(context, session):
      flow_file = session.create()

      if flow_file is not None:
        log.info('created flow file: %s' % flow_file.getAttribute('filename'))
        session.transfer(flow_file, REL_SUCCESS)
  )");

  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("[info] created flow file:"));

  logTestController.reset();
}

TEST_CASE("Python: Test Update Attribute", "[executescriptPythonUpdateAttribute]") {
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

  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptBody.getName(), R"(
    def onTrigger(context, session):
      flow_file = session.get()

      if flow_file is not None:
        log.info('got flow file: %s' % flow_file.getAttribute('filename'))
        flow_file.addAttribute('test_attr', '1')
        attr = flow_file.getAttribute('test_attr')
        log.info('got flow file attr \'test_attr\': %s' % attr)
        flow_file.updateAttribute('test_attr', str(int(attr) + 1))
        session.transfer(flow_file, REL_SUCCESS)
  )");

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir);

  utils::putFileToDir(getFileDir, "tstFile.ext", "tempFile");

  testController.runSession(plan, false);
  testController.runSession(plan, false);
  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("key:test_attr value:2"));

  logTestController.reset();
}

TEST_CASE("Python: Test Get Context Property", "[executescriptPythonGetContextProperty]") {
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

  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptBody.getName(), R"(
    def onTrigger(context, session):
      script_engine = context.getProperty('Script Engine')
      log.info('got Script Engine property: %s' % script_engine)
  )");

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir);

  utils::putFileToDir(getFileDir, "tstFile.ext", "tempFile");

  testController.runSession(plan, false);
  testController.runSession(plan, false);
  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("[info] got Script Engine property: python"));

  logTestController.reset();
}

TEST_CASE("Python: Test Module Directory property", "[executescriptPythonModuleDirectoryProperty]") {
  using org::apache::nifi::minifi::utils::file::concat_path;
  using org::apache::nifi::minifi::utils::file::get_executable_dir;

  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::ExecuteScript>();

  const std::string SCRIPT_FILES_DIRECTORY = minifi::utils::file::getFullPath(concat_path(minifi::utils::file::FileUtils::get_executable_dir(), "test_python_scripts"));
  auto getScriptFullPath = [&SCRIPT_FILES_DIRECTORY](const std::string& script_file_name) {
    return concat_path(SCRIPT_FILES_DIRECTORY, script_file_name);
  };

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);

  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptFile.getName(), getScriptFullPath("foo_bar_processor.py"));
  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ModuleDirectory.getName(), getScriptFullPath(concat_path("foo_modules", "foo.py")) + "," + getScriptFullPath("bar_modules"));

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir);

  utils::putFileToDir(getFileDir, "tstFile.ext", "tempFile");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("foobar"));

  logTestController.reset();
}

TEST_CASE("Python: Non existent script file should throw", "[executescriptPythonNonExistentScriptFile]") {
  TestController testController;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<minifi::processors::ExecuteScript>();

  auto plan = testController.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);

  plan->setProperty(executeScript, minifi::processors::ExecuteScript::ScriptFile.getName(), "/tmp/non-existent-file");

  auto getFileDir = testController.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), getFileDir);

  utils::putFileToDir(getFileDir, "tstFile.ext", "tempFile");

  REQUIRE_THROWS_AS(testController.runSession(plan), minifi::Exception);

  logTestController.reset();
}
