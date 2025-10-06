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

#include "unit/TestBase.h"
#include "unit/Catch.h"

#include "processors/GetFile.h"
#include "ExecutePythonProcessor.h"
#include "processors/LogAttribute.h"
#include "processors/PutFile.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "unit/TestUtils.h"

namespace {
using org::apache::nifi::minifi::test::utils::putFileToDir;
using org::apache::nifi::minifi::test::utils::getFileContent;
using org::apache::nifi::minifi::utils::file::resolve;

class ExecutePythonProcessorTestBase {
 public:
  ExecutePythonProcessorTestBase() :
      logTestController_(LogTestController::getInstance()),
      logger_(logging::LoggerFactory<ExecutePythonProcessorTestBase>::getLogger()) {
    SCRIPT_FILES_DIRECTORY = minifi::utils::file::FileUtils::get_executable_dir() / "resources" / "test_python_scripts";
    reInitialize();
  }

  ExecutePythonProcessorTestBase(ExecutePythonProcessorTestBase&&) = delete;
  ExecutePythonProcessorTestBase(const ExecutePythonProcessorTestBase&) = delete;
  ExecutePythonProcessorTestBase& operator=(ExecutePythonProcessorTestBase&&) = delete;
  ExecutePythonProcessorTestBase& operator=(const ExecutePythonProcessorTestBase&) = delete;

  virtual ~ExecutePythonProcessorTestBase() {
    logTestController_.reset();
  }

 protected:
  void reInitialize() {
    testController_ = std::make_unique<TestController>();
    plan_ = testController_->createPlan();
    logTestController_.setDebug<TestPlan>();
    logTestController_.setDebug<minifi::processors::PutFile>();
  }

  auto getScriptFullPath(const std::filesystem::path& script_file_name) {
    return resolve(SCRIPT_FILES_DIRECTORY, script_file_name);
  }

  static const std::string TEST_FILE_NAME;
  static const std::string TEST_FILE_CONTENT;

  std::filesystem::path SCRIPT_FILES_DIRECTORY;
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
    RUNTIME_RELATIONSHIP_EXCEPTION,
    INITIALIZATION_EXCEPTION
  };

 protected:
  void testSimpleFilePassthrough(const Expectation expectation, const core::Relationship& execute_python_out_conn, const std::string& used_as_script_file) {
    reInitialize();

    auto input_dir = testController_->createTempDirectory();
    putFileToDir(input_dir, TEST_FILE_NAME, TEST_FILE_CONTENT);
    addGetFileProcessorToPlan(input_dir);
    if (Expectation::INITIALIZATION_EXCEPTION == expectation) {
      REQUIRE_THROWS(addExecutePythonProcessorToPlan(used_as_script_file));
      return;
    } else {
      REQUIRE_NOTHROW(addExecutePythonProcessorToPlan(used_as_script_file));
    }

    const auto output_dir = testController_->createTempDirectory();
    addPutFileProcessorToPlan(execute_python_out_conn, output_dir.string());

    plan_->runNextProcessor();  // GetFile
    if (Expectation::RUNTIME_RELATIONSHIP_EXCEPTION == expectation) {
      REQUIRE_THROWS(plan_->runNextProcessor());  // ExecutePythonProcessor
      return;
    }
    REQUIRE_NOTHROW(plan_->runNextProcessor());  // ExecutePythonProcessor
    plan_->runNextProcessor();  // PutFile

    const auto output_file_path = output_dir / TEST_FILE_NAME;

    if (Expectation::OUTPUT_FILE_MATCHES_INPUT == expectation) {
      const std::string output_file_content{ getFileContent(output_file_path) };
      REQUIRE(TEST_FILE_CONTENT == output_file_content);
    }
  }
  void testsStatefulProcessor() {
    reInitialize();
    const auto output_dir = testController_->createTempDirectory();
    addExecutePythonProcessorToPlan(getScriptFullPath("stateful_processor.py"), false);
    addPutFileProcessorToPlan(core::Relationship("success", "description"), output_dir);
    plan_->runNextProcessor();  // ExecutePythonProcessor
    for (std::size_t i = 0; i < 10; ++i) {
      plan_->runCurrentProcessor();  // ExecutePythonProcessor
    }
    plan_->runNextProcessor();  // PutFile
    for (std::size_t i = 0; i < 10; ++i) {
      plan_->runCurrentProcessor();  // PutFile
      const std::string state_name = std::to_string(i);
      const auto output_file_path = output_dir / state_name;
      const std::string output_file_content{ getFileContent(output_file_path) };
      REQUIRE(output_file_content == state_name);
    }
  }

  core::Processor* addGetFileProcessorToPlan(const std::filesystem::path& dir_path) {
    auto getfile = plan_->addProcessor("GetFile", "getfileCreate2");
    plan_->setProperty(getfile, minifi::processors::GetFile::Directory, dir_path.string());
    plan_->setProperty(getfile, minifi::processors::GetFile::KeepSourceFile, "true");
    return getfile;
  }

  core::Processor* addExecutePythonProcessorToPlan(const std::filesystem::path& used_as_script_file, bool link_to_previous = true) {
    auto uuid = utils::IdGenerator::getIdGenerator()->generate();
    auto execute_python_processor = std::make_unique<minifi::extensions::python::processors::ExecutePythonProcessor>(core::ProcessorMetadata{
      .uuid = uuid,
      .name = "executePythonProcessor",
      .logger = logging::LoggerFactory<minifi::extensions::python::processors::ExecutePythonProcessor>::getLogger(uuid)
    });
    execute_python_processor->setScriptFilePath(getScriptFullPath(used_as_script_file).string());
    auto execute_python_processor_unique_ptr = std::make_unique<core::Processor>(execute_python_processor->getName(), execute_python_processor->getUUID(), std::move(execute_python_processor));
    auto processor = plan_->addProcessor(std::move(execute_python_processor_unique_ptr), "executePythonProcessor", core::Relationship("success", "description"), link_to_previous);
    return processor;
  }

  core::Processor* addPutFileProcessorToPlan(const core::Relationship& execute_python_outbound_connection, const std::filesystem::path& dir_path) {
    auto putfile = plan_->addProcessor("PutFile", "putfile", execute_python_outbound_connection, true);
    plan_->setProperty(putfile, minifi::processors::PutFile::Directory, dir_path.string());
    return putfile;
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
  const auto OUTPUT_FILE_MATCHES_INPUT = SimplePythonFlowFileTransferTest::Expectation::OUTPUT_FILE_MATCHES_INPUT;
  const auto RUNTIME_RELATIONSHIP_EXCEPTION = SimplePythonFlowFileTransferTest::Expectation::RUNTIME_RELATIONSHIP_EXCEPTION;
  const auto INITIALIZATION_EXCEPTION = SimplePythonFlowFileTransferTest::Expectation::INITIALIZATION_EXCEPTION;
  // ExecutePython outbound relationships
  const core::Relationship SUCCESS{"success", "description"};
  const core::Relationship FAILURE{"failure", "description"};

  // 0. Neither valid script file nor script body provided
  //                                TEST EXPECTATION  OUT_REL        USE_AS_SCRIPT_FILE
  testSimpleFilePassthrough(INITIALIZATION_EXCEPTION, SUCCESS, "non_existent_script.py");  // NOLINT
  testSimpleFilePassthrough(INITIALIZATION_EXCEPTION, FAILURE, "non_existent_script.py");  // NOLINT

  // 1. Using script file as attribute
  //                                      TEST EXPECTATION  OUT_REL                                 USE_AS_SCRIPT_FILE
  testSimpleFilePassthrough(     OUTPUT_FILE_MATCHES_INPUT, SUCCESS, "passthrough_processor_transfering_to_success.py");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, FAILURE, "passthrough_processor_transfering_to_success.py");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, SUCCESS, "passthrough_processor_transfering_to_failure.py");  // NOLINT
  testSimpleFilePassthrough(     OUTPUT_FILE_MATCHES_INPUT, FAILURE, "passthrough_processor_transfering_to_failure.py");  // NOLINT
  testSimpleFilePassthrough(RUNTIME_RELATIONSHIP_EXCEPTION, FAILURE,                   "non_transferring_processor.py");  // NOLINT
}

TEST_CASE_METHOD(SimplePythonFlowFileTransferTest, "Stateful execution", "[executePythonProcessorStateful]") {
  testsStatefulProcessor();
}

}  // namespace
