/**
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
#include <string>

#include "unit/Catch.h"
#include "../ExecuteProcess.h"
#include "unit/SingleProcessorTestController.h"
#include "utils/file/FileUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class ExecuteProcessTestsFixture {
 public:
  ExecuteProcessTestsFixture()
      : controller_(std::make_unique<processors::ExecuteProcess>("ExecuteProcess")),
        execute_process_(controller_.getProcessor<processors::ExecuteProcess>()) {
    LogTestController::getInstance().setTrace<processors::ExecuteProcess>();
  }
 protected:
  test::SingleProcessorTestController controller_;
  processors::ExecuteProcess* execute_process_;
};

TEST_CASE_METHOD(ExecuteProcessTestsFixture, "ExecuteProcess can run a single command", "[ExecuteProcess]") {
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::Command.name, "echo -n test"));

  controller_.plan->scheduleProcessor(execute_process_);
  auto result = controller_.trigger();

  auto success_flow_files = result.at(processors::ExecuteProcess::Success);
  REQUIRE(success_flow_files.size() == 1);
  CHECK(controller_.plan->getContent(success_flow_files[0]) == "test");
  CHECK(success_flow_files[0]->getAttribute("command") == "echo -n test");
  CHECK(success_flow_files[0]->getAttribute("command.arguments") == "");
}

TEST_CASE_METHOD(ExecuteProcessTestsFixture, "ExecuteProcess can run an executable with a parameter", "[ExecuteProcess]") {
  auto command = minifi::utils::file::get_executable_dir() / "EchoParameters";
  std::string arguments = "0 test_data";
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::Command.name, command.string()));
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::CommandArguments.name, arguments));

  controller_.plan->scheduleProcessor(execute_process_);
  auto result = controller_.trigger();

  auto success_flow_files = result.at(processors::ExecuteProcess::Success);
  REQUIRE(success_flow_files.size() == 1);
  CHECK(controller_.plan->getContent(success_flow_files[0]) == "test_data\n");
  CHECK(success_flow_files[0]->getAttribute("command") == command);
  CHECK(success_flow_files[0]->getAttribute("command.arguments") == arguments);
}

TEST_CASE_METHOD(ExecuteProcessTestsFixture, "ExecuteProcess can run an executable with escaped parameters", "[ExecuteProcess]") {
  auto command = minifi::utils::file::get_executable_dir() / "EchoParameters";
  std::string arguments = R"(0 test_data test_data2 "test data 3" "\"test data 4\")";
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::Command.name, command.string()));
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::CommandArguments.name, arguments));

  controller_.plan->scheduleProcessor(execute_process_);
  auto result = controller_.trigger();

  auto success_flow_files = result.at(processors::ExecuteProcess::Success);
  REQUIRE(success_flow_files.size() == 1);
  CHECK(controller_.plan->getContent(success_flow_files[0]) == "test_data\ntest_data2\ntest data 3\n\"test data 4\"\n");
  CHECK(success_flow_files[0]->getAttribute("command") == command);
  CHECK(success_flow_files[0]->getAttribute("command.arguments") == arguments);
}

TEST_CASE_METHOD(ExecuteProcessTestsFixture, "ExecuteProcess does not produce a flowfile if no output is generated", "[ExecuteProcess]") {
  auto command = minifi::utils::file::get_executable_dir() / "EchoParameters";
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::Command.name, command.string()));

  controller_.plan->scheduleProcessor(execute_process_);
  auto result = controller_.trigger();

  auto success_flow_files = result.at(processors::ExecuteProcess::Success);
  REQUIRE(success_flow_files.empty());
}

TEST_CASE_METHOD(ExecuteProcessTestsFixture, "ExecuteProcess can redirect error stream to stdout", "[ExecuteProcess]") {
  auto command = minifi::utils::file::get_executable_dir() / "EchoParameters";
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::Command.name, command.string()));
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::RedirectErrorStream.name, "true"));

  controller_.plan->scheduleProcessor(execute_process_);
  auto result = controller_.trigger();

  auto success_flow_files = result.at(processors::ExecuteProcess::Success);
  REQUIRE(success_flow_files.size() == 1);
  CHECK(controller_.plan->getContent(success_flow_files[0]) == "Usage: ./EchoParameters <delay between parameters milliseconds> <text to write>\n");
  CHECK(success_flow_files[0]->getAttribute("command") == command);
  CHECK(success_flow_files[0]->getAttribute("command.arguments") == "");
}

TEST_CASE_METHOD(ExecuteProcessTestsFixture, "ExecuteProcess can change workdir", "[ExecuteProcess]") {
  auto command = "./EchoParameters";
  std::string arguments = "0 test_data";
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::Command.name, command));
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::CommandArguments.name, arguments));
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::WorkingDir.name, minifi::utils::file::get_executable_dir().string()));

  controller_.plan->scheduleProcessor(execute_process_);
  auto result = controller_.trigger();

  auto success_flow_files = result.at(processors::ExecuteProcess::Success);
  REQUIRE(success_flow_files.size() == 1);
  CHECK(controller_.plan->getContent(success_flow_files[0]) == "test_data\n");
  CHECK(success_flow_files[0]->getAttribute("command") == command);
  CHECK(success_flow_files[0]->getAttribute("command.arguments") == arguments);
}

TEST_CASE_METHOD(ExecuteProcessTestsFixture, "ExecuteProcess can forward long running output in batches", "[ExecuteProcess]") {
  auto command = minifi::utils::file::get_executable_dir() / "EchoParameters";
  std::string arguments = "100 test_data1 test_data2";
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::Command.name, command.string()));
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::CommandArguments.name, arguments));
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::BatchDuration.name, "10 ms"));

  controller_.plan->scheduleProcessor(execute_process_);
  auto result = controller_.trigger();

  auto success_flow_files = result.at(processors::ExecuteProcess::Success);
  REQUIRE(success_flow_files.size() == 2);
  CHECK(controller_.plan->getContent(success_flow_files[0]) == "test_data1\n");
  CHECK(success_flow_files[0]->getAttribute("command") == command);
  CHECK(success_flow_files[0]->getAttribute("command.arguments") == arguments);
  CHECK(controller_.plan->getContent(success_flow_files[1]) == "test_data2\n");
  CHECK(success_flow_files[1]->getAttribute("command") == command);
  CHECK(success_flow_files[1]->getAttribute("command.arguments") == arguments);
}

TEST_CASE_METHOD(ExecuteProcessTestsFixture, "ExecuteProcess buffer long outputs", "[ExecuteProcess]") {
  auto command = minifi::utils::file::get_executable_dir() / "EchoParameters";
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::Command.name, command.string()));
  std::string param1;

  SECTION("Exact buffer size output") {
    param1.assign(4095, 'a');  // buffer size is 4096, so 4095 'a' characters plus '\n' character should be exactly the buffer size
  }
  SECTION("Larger than buffer size output") {
    param1.assign(8200, 'a');
  }

  std::string arguments = "0 " + param1;
  REQUIRE(execute_process_->setProperty(processors::ExecuteProcess::CommandArguments.name, arguments));

  controller_.plan->scheduleProcessor(execute_process_);
  auto result = controller_.trigger();

  auto success_flow_files = result.at(processors::ExecuteProcess::Success);
  REQUIRE(success_flow_files.size() == 1);
  CHECK(controller_.plan->getContent(success_flow_files[0]) == param1.append("\n"));
  CHECK(success_flow_files[0]->getAttribute("command") == command);
  CHECK(success_flow_files[0]->getAttribute("command.arguments") == arguments);
}
}  // namespace org::apache::nifi::minifi::test
