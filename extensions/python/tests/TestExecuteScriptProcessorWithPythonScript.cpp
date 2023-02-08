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

#include "SingleProcessorTestController.h"
#include "TestBase.h"
#include "Catch.h"

#include "../../script/ExecuteScript.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "utils/TestUtils.h"

namespace org::apache::nifi::minifi::processors::test {

TEST_CASE("Script engine is not set", "[executescriptMisconfiguration]") {
  TestController test_controller;
  auto plan = test_controller.createPlan();

  auto execute_script = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(execute_script, ExecuteScript::ScriptEngine.getName(), "");
  plan->setProperty(execute_script, ExecuteScript::ScriptFile.getName(), "/path/to/script.py");

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Neither script body nor script file is set", "[executescriptMisconfiguration]") {
  TestController test_controller;
  auto plan = test_controller.createPlan();

  auto execute_script = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(execute_script, ExecuteScript::ScriptEngine.getName(), "python");

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Test both script body and script file set", "[executescriptMisconfiguration]") {
  TestController test_controller;
  auto plan = test_controller.createPlan();

  auto execute_script = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(execute_script, ExecuteScript::ScriptEngine.getName(), "python");
  plan->setProperty(execute_script, ExecuteScript::ScriptFile.getName(), "/path/to/script.py");
  plan->setProperty(execute_script, ExecuteScript::ScriptBody.getName(), R"(
    def onTrigger(context, session):
      log.info('hello from python')
  )");

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Python: Test Read File", "[executescriptPythonRead]") {
  TestController test_controller;

  LogTestController& log_test_controller = LogTestController::getInstance();
  log_test_controller.setDebug<TestPlan>();
  log_test_controller.setDebug<minifi::processors::LogAttribute>();
  log_test_controller.setDebug<ExecuteScript>();

  auto plan = test_controller.createPlan();

  auto get_file = plan->addProcessor("GetFile", "getFile");
  auto log_attribute = plan->addProcessor("LogAttribute", "logAttribute",
                                         core::Relationship("success", "description"),
                                         true);
  auto execute_script = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);
  auto put_file = plan->addProcessor("PutFile", "putFile", core::Relationship("success", "description"), true);

  plan->setProperty(execute_script, ExecuteScript::ScriptBody.getName(), R"(
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

  auto get_file_dir = test_controller.createTempDirectory();
  plan->setProperty(get_file, minifi::processors::GetFile::Directory.getName(), get_file_dir.string());

  auto put_file_dir = test_controller.createTempDirectory();
  plan->setProperty(put_file, minifi::processors::PutFile::Directory.getName(), put_file_dir.string());

  test_controller.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  auto path = get_file_dir / "tstFile.ext";
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();

  test_controller.runSession(plan, false);
  test_controller.runSession(plan, false);
  test_controller.runSession(plan, false);

  test_controller.runSession(plan, false);

  std::filesystem::remove(path);

  REQUIRE(log_test_controller.contains("[info] file content: tempFile"));

  // Verify that file content was preserved
  REQUIRE(!std::ifstream(path).good());
  auto moved_file = put_file_dir / "tstFile.ext";
  REQUIRE(std::ifstream(moved_file).good());

  file.open(moved_file, std::ios::in);
  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  REQUIRE("tempFile" == contents);
  file.close();
  log_test_controller.reset();
}

TEST_CASE("Python: Test Write File", "[executescriptPythonWrite]") {
  TestController test_controller;

  LogTestController& log_test_controller = LogTestController::getInstance();
  log_test_controller.setDebug<TestPlan>();
  log_test_controller.setDebug<minifi::processors::LogAttribute>();
  log_test_controller.setDebug<ExecuteScript>();

  auto plan = test_controller.createPlan();

  auto get_file = plan->addProcessor("GetFile", "getFile");
  auto log_attribute = plan->addProcessor("LogAttribute", "logAttribute",
                                         core::Relationship("success", "description"),
                                         true);
  auto execute_script = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);
  auto putFile = plan->addProcessor("PutFile", "putFile", core::Relationship("success", "description"), true);

  plan->setProperty(execute_script, ExecuteScript::ScriptBody.getName(), R"(
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

  auto get_file_dir = test_controller.createTempDirectory();
  plan->setProperty(get_file, minifi::processors::GetFile::Directory.getName(), get_file_dir.string());

  auto put_file_dir = test_controller.createTempDirectory();
  plan->setProperty(putFile, minifi::processors::PutFile::Directory.getName(), put_file_dir.string());

  test_controller.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  auto path = get_file_dir / "tstFile.ext";
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();

  test_controller.runSession(plan, false);
  test_controller.runSession(plan, false);
  test_controller.runSession(plan, false);

  test_controller.runSession(plan, false);

  std::filesystem::remove(path);

  // Verify new content was written
  REQUIRE(!std::ifstream(path).good());
  auto moved_file = put_file_dir / "tstFile.ext";
  REQUIRE(std::ifstream(moved_file).good());

  file.open(moved_file, std::ios::in);
  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  REQUIRE("hello 2" == contents);
  file.close();
  log_test_controller.reset();
}

TEST_CASE("Python: Test Create", "[executescriptPythonCreate]") {
  TestController test_controller;

  LogTestController& log_test_controller = LogTestController::getInstance();
  log_test_controller.setDebug<TestPlan>();
  log_test_controller.setDebug<ExecuteScript>();

  auto plan = test_controller.createPlan();

  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript");

  plan->setProperty(executeScript, ExecuteScript::ScriptBody.getName(), R"(
def onTrigger(context, session):
  flow_file = session.create()

  if flow_file is not None:
    log.info('created flow file: %s' % flow_file.getAttribute('filename'))
    session.transfer(flow_file, REL_SUCCESS)
  )");

  test_controller.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("[info] created flow file:"));

  log_test_controller.reset();
}

TEST_CASE("Python: Test Update Attribute", "[executescriptPythonUpdateAttribute]") {
  TestController test_controller;

  LogTestController& log_test_controller = LogTestController::getInstance();
  log_test_controller.setDebug<TestPlan>();
  log_test_controller.setDebug<minifi::processors::LogAttribute>();
  log_test_controller.setDebug<ExecuteScript>();

  auto plan = test_controller.createPlan();

  auto getFile = plan->addProcessor("GetFile", "getFile");
  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);
  auto logAttribute = plan->addProcessor("LogAttribute", "logAttribute",
                                         core::Relationship("success", "description"),
                                         true);

  plan->setProperty(executeScript, ExecuteScript::ScriptBody.getName(), R"(
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

  auto get_file_dir = test_controller.createTempDirectory();
  plan->setProperty(getFile, minifi::processors::GetFile::Directory.getName(), get_file_dir.string());

  utils::putFileToDir(get_file_dir, "tstFile.ext", "tempFile");

  test_controller.runSession(plan, false);
  test_controller.runSession(plan, false);
  test_controller.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("key:test_attr value:2"));

  log_test_controller.reset();
}

TEST_CASE("Python: Test Get Context Property", "[executescriptPythonGetContextProperty]") {
  TestController test_controller;

  LogTestController& log_test_controller = LogTestController::getInstance();
  log_test_controller.setDebug<TestPlan>();
  log_test_controller.setDebug<minifi::processors::LogAttribute>();
  log_test_controller.setDebug<ExecuteScript>();

  auto plan = test_controller.createPlan();

  auto get_file = plan->addProcessor("GetFile", "getFile");
  auto execute_script = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);
  auto log_attribute = plan->addProcessor("LogAttribute", "logAttribute",
                                         core::Relationship("success", "description"),
                                         true);

  plan->setProperty(execute_script, ExecuteScript::ScriptBody.getName(), R"(
def onTrigger(context, session):
  script_engine = context.getProperty('Script Engine')
  log.info('got Script Engine property: %s' % script_engine)
  )");

  auto get_file_dir = test_controller.createTempDirectory();
  plan->setProperty(get_file, minifi::processors::GetFile::Directory.getName(), get_file_dir.string());

  utils::putFileToDir(get_file_dir, "tstFile.ext", "tempFile");

  test_controller.runSession(plan, false);
  test_controller.runSession(plan, false);
  test_controller.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("[info] got Script Engine property: python"));

  log_test_controller.reset();
}

TEST_CASE("Python: Test Module Directory property", "[executescriptPythonModuleDirectoryProperty]") {
  using org::apache::nifi::minifi::utils::file::get_executable_dir;

  TestController test_controller;

  LogTestController& log_test_controller = LogTestController::getInstance();
  log_test_controller.setDebug<TestPlan>();
  log_test_controller.setDebug<ExecuteScript>();

  const auto script_files_directory = std::filesystem::path(__FILE__).parent_path() / "test_python_scripts";

  auto plan = test_controller.createPlan();

  auto get_file = plan->addProcessor("GetFile", "getFile");
  auto execute_script = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);

  plan->setProperty(execute_script, ExecuteScript::ScriptEngine.getName(), "python");
  plan->setProperty(execute_script, ExecuteScript::ScriptFile.getName(), (script_files_directory / "foo_bar_processor.py").string());
  plan->setProperty(execute_script,
                    ExecuteScript::ModuleDirectory.getName(),
                    (script_files_directory / "foo_modules" / "foo.py").string() + "," + (script_files_directory / "bar_modules").string());

  auto get_file_dir = test_controller.createTempDirectory();
  plan->setProperty(get_file, minifi::processors::GetFile::Directory.getName(), get_file_dir.string());

  utils::putFileToDir(get_file_dir, "tstFile.ext", "tempFile");

  test_controller.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("foobar"));

  log_test_controller.reset();
}

TEST_CASE("Python: Non existent script file should throw", "[executescriptPythonNonExistentScriptFile]") {
  TestController test_controller;

  LogTestController& log_test_controller = LogTestController::getInstance();
  log_test_controller.setDebug<TestPlan>();
  log_test_controller.setDebug<ExecuteScript>();

  auto plan = test_controller.createPlan();

  auto get_file = plan->addProcessor("GetFile", "getFile");
  auto executeScript = plan->addProcessor("ExecuteScript",
                                          "executeScript",
                                          core::Relationship("success", "description"),
                                          true);

  plan->setProperty(executeScript, ExecuteScript::ScriptFile.getName(), "/tmp/non-existent-file");

  auto get_file_dir = test_controller.createTempDirectory();
  plan->setProperty(get_file, minifi::processors::GetFile::Directory.getName(), get_file_dir.string());

  utils::putFileToDir(get_file_dir, "tstFile.ext", "tempFile");

  REQUIRE_THROWS_AS(test_controller.runSession(plan), minifi::Exception);

  log_test_controller.reset();
}

TEST_CASE("Python can remove flowfiles", "[ExecuteScript]") {
  const auto execute_script = std::make_shared<ExecuteScript>("ExecuteScript");

  minifi::test::SingleProcessorTestController controller{execute_script};
  LogTestController::getInstance().setTrace<ExecuteScript>();
  execute_script->setProperty(ExecuteScript::ScriptEngine, "python");
  execute_script->setProperty(ExecuteScript::ScriptBody.getName(), R"(
def onTrigger(context, session):
  flow_file = session.get()
  session.remove(flow_file);)");
  REQUIRE_NOTHROW(controller.trigger("hello"));
}

}  // namespace org::apache::nifi::minifi::processors::test
