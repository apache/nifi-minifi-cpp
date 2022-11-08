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
#include <array>
#include <memory>
#include <string>
#include <set>

#include "TestBase.h"
#include "Catch.h"

#include "processors/GetFile.h"
#include "python/ExecutePythonProcessor.h"
#include "processors/LogAttribute.h"
#include "processors/PutFile.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "utils/TestUtils.h"

namespace {
using org::apache::nifi::minifi::utils::putFileToDir;
using org::apache::nifi::minifi::utils::getFileContent;
using org::apache::nifi::minifi::utils::file::getFileNameAndPath;
using org::apache::nifi::minifi::utils::file::concat_path;
using org::apache::nifi::minifi::utils::file::resolve;
using org::apache::nifi::minifi::utils::file::getFullPath;

class ExecutePythonProcessorTestBase {
 public:
  ExecutePythonProcessorTestBase() :
      logTestController_(LogTestController::getInstance()),
      logger_(logging::LoggerFactory<ExecutePythonProcessorTestBase>::getLogger()) {
    std::string path;
    std::string filename;
    getFileNameAndPath(__FILE__, path, filename);
    SCRIPT_FILES_DIRECTORY = getFullPath(concat_path(minifi::utils::file::FileUtils::get_executable_dir(), "test_python_scripts"));
    reInitialize();
  }
  virtual ~ExecutePythonProcessorTestBase() {
    logTestController_.reset();
  }

 protected:
  void reInitialize() {
    testController_ = std::make_unique<TestController>();
    plan_ = testController_->createPlan();
    logTestController_.setDebug<TestPlan>();
    logTestController_.setDebug<minifi::processors::PutFile>();
    logTestController_.setDebug<minifi::processors::PutFile::ReadCallback>();
  }

  std::string getScriptFullPath(const std::string& script_file_name) {
    return resolve(SCRIPT_FILES_DIRECTORY, script_file_name);
  }

  static const std::string TEST_FILE_NAME;
  static const std::string TEST_FILE_CONTENT;

  std::string SCRIPT_FILES_DIRECTORY;
  std::unique_ptr<TestController> testController_;
  std::shared_ptr<TestPlan> plan_;
  LogTestController& logTestController_;
  std::shared_ptr<logging::Logger> logger_;
};

const std::string ExecutePythonProcessorTestBase::TEST_FILE_NAME{ "test_file.txt" };
const std::string ExecutePythonProcessorTestBase::TEST_FILE_CONTENT{ "Test text\n" };

class SimplePythonFlowFileTransferTest : public ExecutePythonProcessorTestBase {
 public:
  enum class Expectation {
    OUTPUT_FILE_MATCHES_INPUT,
    RUNTIME_RELATIONSHIP_EXCEPTION
  };

 protected:
  void testSimpleFilePassthrough(const Expectation expectation, const core::Relationship& execute_python_out_conn, const std::string& used_as_script_file, const std::string& used_as_script_body) {
    reInitialize();

    const std::string input_dir = testController_->createTempDirectory();
    putFileToDir(input_dir, TEST_FILE_NAME, TEST_FILE_CONTENT);
    addGetFileProcessorToPlan(input_dir);
    REQUIRE_NOTHROW(addExecutePythonProcessorToPlan(used_as_script_file, used_as_script_body));

    const std::string output_dir = testController_->createTempDirectory();
    addPutFileProcessorToPlan(execute_python_out_conn, output_dir);

    plan_->runNextProcessor();  // GetFile
    if (Expectation::RUNTIME_RELATIONSHIP_EXCEPTION == expectation) {
      REQUIRE_THROWS(plan_->runNextProcessor());  // ExecutePythonProcessor
      return;
    }
    REQUIRE_NOTHROW(plan_->runNextProcessor());  // ExecutePythonProcessor
    plan_->runNextProcessor();  // PutFile

    const std::string output_file_path = output_dir + utils::file::FileUtils::get_separator() +  TEST_FILE_NAME;

    if (Expectation::OUTPUT_FILE_MATCHES_INPUT == expectation) {
      const std::string output_file_content{ getFileContent(output_file_path) };
      REQUIRE(TEST_FILE_CONTENT == output_file_content);
    }
  }
  void testsStatefulProcessor() {
    reInitialize();
    const std::string output_dir = testController_->createTempDirectory();

    auto executePythonProcessor = plan_->addProcessor("ExecutePythonProcessor", "executePythonProcessor");
    plan_->setProperty(executePythonProcessor, "Script File", getScriptFullPath("stateful_processor.py"));

    addPutFileProcessorToPlan(core::Relationship("success", "description"), output_dir);
    plan_->runNextProcessor();  // ExecutePythonProcessor
    for (std::size_t i = 0; i < 10; ++i) {
      plan_->runCurrentProcessor();  // ExecutePythonProcessor
    }
    plan_->runNextProcessor();  // PutFile
    for (std::size_t i = 0; i < 10; ++i) {
      plan_->runCurrentProcessor();  // PutFile
      const std::string state_name = std::to_string(i);
      const std::string output_file_path = concat_path(output_dir, state_name);
      const std::string output_file_content{ getFileContent(output_file_path) };
      REQUIRE(output_file_content == state_name);
    }
  }

  std::shared_ptr<core::Processor> addGetFileProcessorToPlan(const std::string& dir_path) {
    std::shared_ptr<core::Processor> getfile = plan_->addProcessor("GetFile", "getfileCreate2");
    plan_->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir_path);
    plan_->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile.getName(), "true");
    return getfile;
  }

  std::shared_ptr<core::Processor> addExecutePythonProcessorToPlan(const std::string& used_as_script_file, const std::string& used_as_script_body) {
    auto executePythonProcessor = plan_->addProcessor("ExecutePythonProcessor", "executePythonProcessor", core::Relationship("success", "description"), true);
    if (!used_as_script_file.empty()) {
      plan_->setProperty(executePythonProcessor, "Script File", getScriptFullPath(used_as_script_file));
    }
    if (!used_as_script_body.empty()) {
      plan_->setProperty(executePythonProcessor, "Script Body", getFileContent(getScriptFullPath(used_as_script_body)));
    }
    return executePythonProcessor;
  }

  std::shared_ptr<core::Processor> addPutFileProcessorToPlan(const core::Relationship& execute_python_outbound_connection, const std::string& dir_path) {
    std::shared_ptr<core::Processor> putfile = plan_->addProcessor("PutFile", "putfile", execute_python_outbound_connection, true);
    plan_->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir_path);
    return putfile;
  }

  void testReloadOnScriptProperty(std::optional<bool> reload_on_script_change, uint32_t expected_success_file_count, uint32_t expected_failure_file_count) {
    const std::string input_dir = testController_->createTempDirectory();
    putFileToDir(input_dir, TEST_FILE_NAME, TEST_FILE_CONTENT);
    addGetFileProcessorToPlan(input_dir);
    auto script_content{ getFileContent(getScriptFullPath("passthrough_processor_transfering_to_success.py")) };
    const std::string reloaded_script_dir = testController_->createTempDirectory();
    putFileToDir(reloaded_script_dir, "reloaded_script.py", script_content);

    auto execute_python_processor = addExecutePythonProcessorToPlan(concat_path(reloaded_script_dir, "reloaded_script.py"), "");
    if (reload_on_script_change) {
      plan_->setProperty(execute_python_processor, "Reload on Script Change", *reload_on_script_change ? "true" : "false");
    }

    auto success_putfile = plan_->addProcessor("PutFile", "SuccessPutFile", { {"success", "d"} }, false);
    plan_->addConnection(execute_python_processor, {"success", "d"}, success_putfile);
    success_putfile->setAutoTerminatedRelationships(std::array{core::Relationship{"success", "d"}, core::Relationship{"failure", "d"}});
    auto success_output_dir = testController_->createTempDirectory();
    plan_->setProperty(success_putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), success_output_dir);

    auto failure_putfile = plan_->addProcessor("PutFile", "FailurePutFile", { {"success", "d"} }, false);
    plan_->addConnection(execute_python_processor, {"failure", "d"}, failure_putfile);
    failure_putfile->setAutoTerminatedRelationships(std::array{core::Relationship{"success", "d"}, core::Relationship{"failure", "d"}});
    auto failure_output_dir = testController_->createTempDirectory();
    plan_->setProperty(failure_putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), failure_output_dir);

    testController_->runSession(plan_);
    plan_->reset();
    script_content = getFileContent(getScriptFullPath("passthrough_processor_transfering_to_failure.py"));
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));  // make sure the file gets newer modification time
    putFileToDir(reloaded_script_dir, "reloaded_script.py", script_content);
    testController_->runSession(plan_);

    std::vector<std::string> file_contents;

    auto lambda = [&file_contents](const std::string& path, const std::string& filename) -> bool {
      std::ifstream is(path + utils::file::FileUtils::get_separator() + filename, std::ifstream::binary);
      file_contents.push_back(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()));
      return true;
    };

    utils::file::FileUtils::list_dir(success_output_dir, lambda, plan_->getLogger(), false);

    REQUIRE(file_contents.size() == expected_success_file_count);

    file_contents.clear();
    utils::file::FileUtils::list_dir(failure_output_dir, lambda, plan_->getLogger(), false);

    REQUIRE(file_contents.size() == expected_failure_file_count);
  }
};

// Test for python processors for simple passthrough cases
//
// testSimpleFilePassthrough(OUTPUT_FILE_MATCHES_INPUT, SUCCESS, "", "passthrough_processor_transfering_to_success.py");
//
// translates to
//
// GIVEN an ExecutePythonProcessor set up with a "Script Body" attribute that transfers to REL_SUCCESS, but not "Script File"
// WHEN the processor is triggered
// THEN any consumer using the success relationship as source should receive the transfered data
//
TEST_CASE_METHOD(SimplePythonFlowFileTransferTest, "Simple file passthrough", "[executePythonProcessorSimple]") {
  // Expectations
  const auto OUTPUT_FILE_MATCHES_INPUT          = SimplePythonFlowFileTransferTest::Expectation::OUTPUT_FILE_MATCHES_INPUT;
  const auto RUNTIME_RELATIONSHIP_EXCEPTION    = SimplePythonFlowFileTransferTest::Expectation::RUNTIME_RELATIONSHIP_EXCEPTION;
  // ExecutePython outbound relationships
  const core::Relationship SUCCESS {"success", "description"};
  const core::Relationship FAILURE{"failure", "description"};

  // For the tests "" is treated as none-provided since no optional implementation was ported to the project yet

  // 0. Neither valid script file nor script body provided
  //                                      TEST EXPECTATION  OUT_REL        USE_AS_SCRIPT_FILE  USE_AS_SCRIPT_BODY
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, SUCCESS,                       "", "");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, FAILURE,                       "", "");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, SUCCESS, "non_existent_script.py", "");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, FAILURE, "non_existent_script.py", "");  // NOLINT

  // 1. Using script file as attribute
  //                                      TEST EXPECTATION  OUT_REL                                 USE_AS_SCRIPT_FILE  USE_AS_SCRIPT_BODY
  testSimpleFilePassthrough(     OUTPUT_FILE_MATCHES_INPUT, SUCCESS, "passthrough_processor_transfering_to_success.py", "");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, FAILURE, "passthrough_processor_transfering_to_success.py", "");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, SUCCESS, "passthrough_processor_transfering_to_failure.py", "");  // NOLINT
  testSimpleFilePassthrough(     OUTPUT_FILE_MATCHES_INPUT, FAILURE, "passthrough_processor_transfering_to_failure.py", "");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, FAILURE,                   "non_transferring_processor.py", "");  // NOLINT

  // 2. Using script body as attribute
  //                                      TEST EXPECTATION  OUT_REL  SCRIPT_FILE                        USE_AS_SCRIPT_BODY
  testSimpleFilePassthrough(     OUTPUT_FILE_MATCHES_INPUT, SUCCESS, "", "passthrough_processor_transfering_to_success.py");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, FAILURE, "", "passthrough_processor_transfering_to_success.py");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, SUCCESS, "", "passthrough_processor_transfering_to_failure.py");  // NOLINT
  testSimpleFilePassthrough(     OUTPUT_FILE_MATCHES_INPUT, FAILURE, "", "passthrough_processor_transfering_to_failure.py");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, FAILURE, "",                   "non_transferring_processor.py");  // NOLINT

  // 3. Setting both attributes
  //                                      TEST EXPECTATION  OUT_REL                                        SCRIPT_FILE                                 USE_AS_SCRIPT_BODY
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, SUCCESS, "passthrough_processor_transfering_to_success.py", "passthrough_processor_transfering_to_success.py");  // NOLINT
}

TEST_CASE_METHOD(SimplePythonFlowFileTransferTest, "Stateful execution", "[executePythonProcessorStateful]") {
  testsStatefulProcessor();
}

TEST_CASE_METHOD(SimplePythonFlowFileTransferTest, "Test the Reload On Script Change property", "[executePythonProcessorReloadScript]") {
  SECTION("When Reload On Script Change is not set the script is reloaded by default") {
    const uint32_t EXPECTED_SUCCESS_FILE_COUNT = 1;
    const uint32_t EXPECTED_FAILURE_FILE_COUNT = 1;
    testReloadOnScriptProperty(std::nullopt, EXPECTED_SUCCESS_FILE_COUNT, EXPECTED_FAILURE_FILE_COUNT);
  }

  SECTION("When Reload On Script Change is set to true both transfer to success and failure scripts are run") {
    const uint32_t EXPECTED_SUCCESS_FILE_COUNT = 1;
    const uint32_t EXPECTED_FAILURE_FILE_COUNT = 1;
    testReloadOnScriptProperty(true, EXPECTED_SUCCESS_FILE_COUNT, EXPECTED_FAILURE_FILE_COUNT);
  }

  SECTION("When Reload On Script Change is set to false only transfer to success script is run") {
    const uint32_t EXPECTED_SUCCESS_FILE_COUNT = 1;
    const uint32_t EXPECTED_FAILURE_FILE_COUNT = 0;
    testReloadOnScriptProperty(false, EXPECTED_SUCCESS_FILE_COUNT, EXPECTED_FAILURE_FILE_COUNT);
  }
}

TEST_CASE_METHOD(SimplePythonFlowFileTransferTest, "Test module load of processor", "[executePythonProcessorModuleLoad]") {
  const std::string input_dir = testController_->createTempDirectory();
  putFileToDir(input_dir, TEST_FILE_NAME, TEST_FILE_CONTENT);
  addGetFileProcessorToPlan(input_dir);

  auto execute_python_processor = addExecutePythonProcessorToPlan("foo_bar_processor.py", "");
  plan_->setProperty(execute_python_processor, "Module Directory", getScriptFullPath(concat_path("foo_modules", "foo.py")) + "," + getScriptFullPath("bar_modules"));

  auto success_putfile = plan_->addProcessor("PutFile", "SuccessPutFile", { {"success", "d"} }, false);
  plan_->addConnection(execute_python_processor, {"success", "d"}, success_putfile);
  success_putfile->setAutoTerminatedRelationships(std::array{core::Relationship{"success", "d"}, core::Relationship{"failure", "d"}});
  auto success_output_dir = testController_->createTempDirectory();
  plan_->setProperty(success_putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), success_output_dir);

  testController_->runSession(plan_);
  plan_->reset();

  std::vector<std::string> file_contents;

  auto lambda = [&file_contents](const std::string& path, const std::string& filename) -> bool {
    std::ifstream is(path + utils::file::FileUtils::get_separator() + filename, std::ifstream::binary);
    file_contents.push_back(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()));
    return true;
  };

  utils::file::FileUtils::list_dir(success_output_dir, lambda, plan_->getLogger(), false);

  REQUIRE(file_contents.size() == 1);
}

}  // namespace
