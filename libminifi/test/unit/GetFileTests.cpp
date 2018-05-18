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
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <fstream>


#include "../TestBase.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"

TEST_CASE("GetFile: FIFO", "[getFileFifo]") { // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Define directory for input
  std::string in_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&in_dir[0]) != nullptr);

  // Define test input file
  std::string in_file(in_dir);
  in_file.append("/testfifo");

  // Build MiNiFi processing graph
  auto get_file = plan->addProcessor(
      "GetFile",
      "Get");
  plan->setProperty(
      get_file,
      processors::GetFile::Directory.getName(), in_dir);
  plan->setProperty(
      get_file,
      processors::GetFile::KeepSourceFile.getName(),
      "true");
  plan->addProcessor(
      "LogAttribute",
      "Log",
      core::Relationship("success", "description"),
      true);

  // Write test input
  REQUIRE(0 == mkfifo(in_file.c_str(), 0777));

  // Run test flow
  std::thread write_thread([&] {
    std::ofstream in_file_stream(in_file);
    in_file_stream << "The quick brown fox jumps over the lazy dog" << std::endl;
  });

  plan->runNextProcessor();  // Get
  plan->runNextProcessor();  // Log

  write_thread.join();

  // Check log output
  REQUIRE(LogTestController::getInstance().contains("key:flow.id"));
  REQUIRE(LogTestController::getInstance().contains("Size:44 Offset:0"));
}

