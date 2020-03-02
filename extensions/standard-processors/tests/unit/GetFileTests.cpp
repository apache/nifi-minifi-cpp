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

#include "TestBase.h"
#include "LogAttribute.h"
#include "GetFile.h"

#ifdef WIN32
#include <fileapi.h>
#endif

/**
 * This is an invalidly named test as we can't guarantee order, nor shall we.
 */
TEST_CASE("GetFile: MaxSize", "[getFileFifo]") {  // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Define directory for input
  char in_dir[] = "/tmp/gt.XXXXXX";
  auto temp_path = testController.createTempDirectory(in_dir);
  REQUIRE(!temp_path.empty());

  // Define test input file
  std::string in_file(temp_path + utils::file::FileUtils::get_separator() + "testfifo");
  std::string hidden_in_file(temp_path + utils::file::FileUtils::get_separator() + ".testfifo");

  // Build MiNiFi processing graph

  auto get_file = plan->addProcessor("GetFile", "Get");
  plan->setProperty(get_file, processors::GetFile::Directory.getName(), temp_path);
  plan->setProperty(get_file, processors::GetFile::KeepSourceFile.getName(), "true");
  plan->setProperty(get_file, processors::GetFile::MaxSize.getName(), "50 B");
  plan->setProperty(get_file, processors::GetFile::IgnoreHiddenFile.getName(), "true");
  auto log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");

  // Write test input.
  std::ofstream in_file_stream(in_file);
  in_file_stream << "The quick brown fox jumps over the lazy dog" << std::endl;
  in_file_stream.close();

  std::this_thread::sleep_for(std::chrono::seconds(2));

  in_file_stream.open(in_file + "2");
  in_file_stream << "The quick brown fox jumps over the lazy dog who is 2 legit to quit" << std::endl;
  in_file_stream.close();

  std::ofstream hidden_in_file_stream(hidden_in_file);
  hidden_in_file_stream << "But noone has ever seen it" << std::endl;
  hidden_in_file_stream.close();
#ifdef WIN32
  const auto hide_file_err = FileUtils::hide_file(hidden_in_file.c_str());
  REQUIRE(!hide_file_err);
#endif
  plan->runNextProcessor();  // Get
  plan->runNextProcessor();  // Log

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));  // The hidden and the too big files should be ignored
  // Check log output on windows std::endl; will produce \r\n can write manually but might as well just
  // account for the size difference here
  REQUIRE(LogTestController::getInstance().contains("key:flow.id"));
#ifdef WIN32
  REQUIRE(LogTestController::getInstance().contains("Size:45 Offset:0"));
#else
  REQUIRE(LogTestController::getInstance().contains("Size:44 Offset:0"));
#endif
}

