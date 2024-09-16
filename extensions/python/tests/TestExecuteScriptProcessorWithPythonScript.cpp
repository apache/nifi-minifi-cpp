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

#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

#include "../../script/ExecuteScript.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"

namespace org::apache::nifi::minifi::processors::test {

TEST_CASE("Python: hello world") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "python");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(print("Hello world!"))");

  CHECK_NOTHROW(controller.trigger());
}

TEST_CASE("Script engine is not set", "[executescriptMisconfiguration]") {
  TestController test_controller;
  auto plan = test_controller.createPlan();

  auto execute_script = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(execute_script, ExecuteScript::ScriptEngine, "");
  plan->setProperty(execute_script, ExecuteScript::ScriptFile, "/path/to/script.py");

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Neither script body nor script file is set", "[executescriptMisconfiguration]") {
  TestController test_controller;
  auto plan = test_controller.createPlan();

  auto execute_script = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(execute_script, ExecuteScript::ScriptEngine, "python");

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Test both script body and script file set", "[executescriptMisconfiguration]") {
  TestController test_controller;
  auto plan = test_controller.createPlan();

  auto execute_script = plan->addProcessor("ExecuteScript", "executeScript");

  plan->setProperty(execute_script, ExecuteScript::ScriptEngine, "python");
  plan->setProperty(execute_script, ExecuteScript::ScriptFile, "/path/to/script.py");
  plan->setProperty(execute_script, ExecuteScript::ScriptBody, R"(
    def onTrigger(context, session):
      log.info('hello from python')
  )");

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE("Python: Test session get should return None if there are no flowfiles in the incoming connections") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "python");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
def onTrigger(context, session):
  flow_file = session.get()

  if flow_file is not None:
    raise Exception("Didn't expect flow_file")
  )");
  auto result = controller.trigger();
  REQUIRE(result.at(ExecuteScript::Success).empty());
  REQUIRE(result.at(ExecuteScript::Failure).empty());
}

TEST_CASE("Python: Test Read File", "[executescriptPythonRead]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "python");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
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

  auto result = controller.trigger("tempFile");
  REQUIRE(result.at(ExecuteScript::Success).size() == 1);
  CHECK(controller.plan->getContent(result.at(ExecuteScript::Success)[0]) == "tempFile");
}

TEST_CASE("Python: Test Write File", "[executescriptPythonWrite]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "python");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
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

  auto result = controller.trigger("tempFile");
  REQUIRE(result.at(ExecuteScript::Success).size() == 1);
  CHECK(controller.plan->getContent(result.at(ExecuteScript::Success)[0]) == "hello 2");
}

TEST_CASE("Python: Test Create", "[executescriptPythonCreate]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "python");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
def onTrigger(context, session):
  flow_file = session.create()

  if flow_file is not None:
    log.info('created flow file: %s' % flow_file.getAttribute('filename'))
    session.transfer(flow_file, REL_SUCCESS)
  )");


  auto result = controller.trigger();
  REQUIRE(result.at(ExecuteScript::Success).size() == 1);
  REQUIRE(result.at(ExecuteScript::Failure).empty());
  REQUIRE(LogTestController::getInstance().contains("[info] created flow file:"));
}

TEST_CASE("Python: Test Update Attribute", "[executescriptPythonUpdateAttribute]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "python");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
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

  auto result = controller.trigger("tempFile");
  REQUIRE(result.at(ExecuteScript::Success).size() == 1);
  CHECK(controller.plan->getContent(result.at(ExecuteScript::Success)[0]) == "tempFile");
  CHECK(result.at(ExecuteScript::Success)[0]->getAttribute("test_attr") == "2");
}

TEST_CASE("Python: Test Get Context Property", "[executescriptPythonGetContextProperty]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "python");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
def onTrigger(context, session):
  script_engine = context.getProperty('Script Engine')
  log.info('got Script Engine property: %s' % script_engine)
  )");

  auto result_without_input = controller.trigger();
  REQUIRE(result_without_input.at(ExecuteScript::Success).empty());
  REQUIRE(result_without_input.at(ExecuteScript::Failure).empty());

  REQUIRE(LogTestController::getInstance().contains("[info] got Script Engine property: python"));
}

TEST_CASE("Python: Test Module Directory property", "[executescriptPythonModuleDirectoryProperty]") {
  using org::apache::nifi::minifi::utils::file::get_executable_dir;

  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  const auto script_files_directory =  minifi::utils::file::FileUtils::get_executable_dir() / "resources" / "test_python_scripts";

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "python");
  execute_script->setProperty(ExecuteScript::ScriptFile.name, (script_files_directory / "foo_bar_processor.py").string());
  execute_script->setProperty(ExecuteScript::ModuleDirectory.name, (script_files_directory / "foo_modules" / "foo.py").string() + "," + (script_files_directory / "bar_modules").string());

  auto result = controller.trigger("tempFile");
  REQUIRE(result.at(ExecuteScript::Success).size() == 1);
  REQUIRE(result.at(ExecuteScript::Failure).empty());

  REQUIRE(LogTestController::getInstance().contains("foobar"));
}

TEST_CASE("Python: Non existent script file should throw", "[executescriptPythonNonExistentScriptFile]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();

  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "python");
  execute_script->setProperty(ExecuteScript::ScriptFile.name, "/tmp/non-existent-file");

  REQUIRE_THROWS_AS(controller.trigger("tempFile"), minifi::Exception);
}

TEST_CASE("Python can remove flowfiles", "[ExecuteScript]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<ExecuteScript>();
  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "python");
  execute_script->setProperty(ExecuteScript::ScriptBody.name, R"(
def onTrigger(context, session):
  flow_file = session.get()
  session.remove(flow_file))");
  auto result = controller.trigger("hello");
  REQUIRE(result.at(ExecuteScript::Success).empty());
  REQUIRE(result.at(ExecuteScript::Failure).empty());
}

TEST_CASE("Python can store states in StateManager", "[ExecuteScript]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<ExecuteScript>("ExecuteScript")};
  const auto execute_script = controller.getProcessor();
  LogTestController::getInstance().setTrace<minifi::processors::ExecuteScript>();
  execute_script->setProperty(ExecuteScript::ScriptEngine.name, "python");
  execute_script->setProperty(ExecuteScript::ScriptBody.name,
      R"(
def onTrigger(context, session):
  state_manager = context.getStateManager()
  state = state_manager.get()
  if state is None:
    state = {}
    state['python_trigger_count'] = 0
  python_trigger_count = state['python_trigger_count']
  log.info('python_trigger_count: ' + str(python_trigger_count))
  state['python_trigger_count'] =str(int(python_trigger_count) + 1)
  state_manager.set(state)
)");

  for (size_t i = 0; i < 4; ++i) {
    controller.trigger();
    CHECK(LogTestController::getInstance().contains(fmt::format("python_trigger_count: {}", i)));
  }
}

}  // namespace org::apache::nifi::minifi::processors::test
