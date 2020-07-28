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

#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <iostream>
#include <set>
#include <algorithm>
#include <random>
#include <cstdlib>
#include "FlowController.h"
#include "TestBase.h"
#include "core/Core.h"
#include "core/FlowFile.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "TailFile.h"
#include "LogAttribute.h"

static std::string NEWLINE_FILE = ""  // NOLINT
        "one,two,three\n"
        "four,five,six, seven";
static const char *TMP_FILE = "minifi-tmpfile.txt";
static const char *STATE_FILE = "minifi-state-file.txt";

namespace {
std::string createTempFile(const std::string &directory, const std::string &file_name, const std::string &contents,
    std::ios_base::openmode open_mode = std::ios::out | std::ios::binary) {
  std::string full_file_name = directory + utils::file::FileUtils::get_separator() + file_name;
  std::ofstream tmpfile{full_file_name, open_mode};
  tmpfile << contents;
  return full_file_name;
}

void appendTempFile(const std::string &directory, const std::string &file_name, const std::string &contents,
    std::ios_base::openmode open_mode = std::ios::app | std::ios::binary) {
  createTempFile(directory, file_name, contents, open_mode);
}

void removeFile(const std::string &directory, const std::string &file_name) {
  std::string full_file_name = directory + utils::file::FileUtils::get_separator() + file_name;
  std::remove(full_file_name.c_str());
}

void renameTempFile(const std::string &directory, const std::string &old_file_name, const std::string &new_file_name) {
  std::string old_full_file_name = directory + utils::file::FileUtils::get_separator() + old_file_name;
  std::string new_full_file_name = directory + utils::file::FileUtils::get_separator() + new_file_name;
  rename(old_full_file_name.c_str(), new_full_file_name.c_str());
}
}  // namespace

TEST_CASE("TailFile reads the file until the first delimiter", "[simple]") {
  // Create and write to the test file

  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto id = tailfile->getUUIDStr();
  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::stringstream temp_file;
  temp_file << dir << utils::file::FileUtils::get_separator() << TMP_FILE;

  std::ofstream tmpfile;
  tmpfile.open(temp_file.str(), std::ios::out | std::ios::binary);
  tmpfile << NEWLINE_FILE;
  tmpfile.close();

  std::stringstream state_file;
  state_file << dir << utils::file::FileUtils::get_separator() << STATE_FILE;

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  testController.runSession(plan, false);
  auto records = plan->getProvenanceRecords();
  REQUIRE(records.size() == 5);  // line 1: CREATE, MODIFY; line 2: CREATE, MODIFY, DROP

  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
  REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.find_first_of('\n') + 1) + " Offset:0"));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile picks up the second line if a delimiter is written between runs", "[state]") {
  // Create and write to the test file
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto id = tailfile->getUUIDStr();

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::stringstream temp_file;
  temp_file << dir << utils::file::FileUtils::get_separator() << TMP_FILE;

  std::ofstream tmpfile;
  tmpfile.open(temp_file.str(), std::ios::out | std::ios::binary);
  tmpfile << NEWLINE_FILE;
  tmpfile.close();

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.0-13.txt"));

  plan->reset(true);  // start a new but with state file
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  std::ofstream appendStream;
  appendStream.open(temp_file.str(), std::ios_base::app | std::ios_base::binary);
  appendStream << std::endl;
  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.14-34.txt"));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile re-reads the file if the state is deleted between runs", "[state]") {
  // Create and write to the test file

  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto id = tailfile->getUUIDStr();

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::stringstream temp_file;
  temp_file << dir << utils::file::FileUtils::get_separator() << TMP_FILE;

  std::ofstream tmpfile;
  tmpfile.open(temp_file.str(), std::ios::out | std::ios::binary);
  tmpfile << NEWLINE_FILE;
  tmpfile.close();

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.0-13.txt"));

  plan->reset(true);  // start a new but with state file
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  plan->getStateManagerProvider()->getCoreComponentStateManager(*tailfile)->clear();

  testController.runSession(plan, true);

  // if we lose state we restart
  REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.0-13.txt"));
}

TEST_CASE("TailFile picks up the state correctly if it is rewritten between runs", "[state]") {
  // Create and write to the test file
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto id = tailfile->getUUIDStr();

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);
  std::stringstream temp_file;
  temp_file << dir << utils::file::FileUtils::get_separator() << TMP_FILE;

  std::ofstream tmpfile;
  tmpfile.open(temp_file.str(), std::ios::out | std::ios::binary);
  tmpfile << NEWLINE_FILE;
  tmpfile.close();

  std::ofstream appendStream;
  appendStream.open(temp_file.str(), std::ios_base::app | std::ios_base::binary);
  appendStream.write("\n", 1);
  appendStream.close();

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  testController.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.0-13.txt"));

  std::string filePath, fileName;
  REQUIRE(utils::file::PathUtils::getFileNameAndPath(temp_file.str(), filePath, fileName));

  // should stay the same
  for (int i = 0; i < 5; i++) {
    plan->reset(true);  // start a new but with state file
    LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

    plan->getStateManagerProvider()->getCoreComponentStateManager(*tailfile)->set({{"file.0.name", fileName},
                                                                                   {"file.0.position", "14"},
                                                                                   {"file.0.current", temp_file.str()}});

    testController.runSession(plan, true);

    // if we lose state we restart
    REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.14-34.txt"));
  }
  for (int i = 14; i < 34; i++) {
    plan->reset(true);  // start a new but with state file

    plan->getStateManagerProvider()->getCoreComponentStateManager(*tailfile)->set({{"file.0.name", fileName},
                                                                                   {"file.0.position", std::to_string(i)},
                                                                                   {"file.0.current", temp_file.str()}});

    testController.runSession(plan, true);
  }

  plan->runCurrentProcessor();
  for (int i = 14; i < 34; i++) {
    REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile." + std::to_string(i) + "-34.txt"));
  }
}

TEST_CASE("TailFile converts the old-style state file to the new-style state", "[state][migration]") {
  // Create and write to the test file
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  auto plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto id = tailfile->getUUIDStr();

  auto logattribute = plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);
  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog.getName(), "0");

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::stringstream state_file;
  state_file << dir << utils::file::FileUtils::get_separator() << STATE_FILE;

  auto statefile = state_file.str() + "." + id;

  SECTION("single") {
    const std::string temp_file = createTempFile(dir, TMP_FILE, NEWLINE_FILE + '\n');

    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file);
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), state_file.str());
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

    std::ofstream newstatefile;
    newstatefile.open(statefile);
    SECTION("legacy") {
      newstatefile << "FILENAME=" << temp_file << std::endl;
      newstatefile << "POSITION=14" << std::endl;
    }
    SECTION("newer single") {
      newstatefile << "FILENAME=" << TMP_FILE << std::endl;
      newstatefile << "POSITION." << TMP_FILE << "=14" << std::endl;
      newstatefile << "CURRENT." << TMP_FILE << "=" << temp_file << std::endl;
    }
    newstatefile.close();

    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.14-34.txt"));

    std::unordered_map<std::string, std::string> state;
    REQUIRE(plan->getStateManagerProvider()->getCoreComponentStateManager(*tailfile)->get(state));

    std::string filePath, fileName;
    REQUIRE(utils::file::PathUtils::getFileNameAndPath(temp_file, filePath, fileName));
    std::unordered_map<std::string, std::string> expected_state{{"file.0.name", fileName},
                                                                {"file.0.position", "35"},
                                                                {"file.0.current", temp_file},
                                                                {"file.0.checksum", "1404369522"}};
    for (const auto& key_value_pair : expected_state) {
      const auto it = state.find(key_value_pair.first);
      REQUIRE(it != state.end());
      REQUIRE(it->second == key_value_pair.second);
    }
    REQUIRE(state.find("file.0.last_read_time") != state.end());
  }

  SECTION("multiple") {
    const std::string file_name_1 = "bar.txt";
    const std::string file_name_2 = "foo.txt";
    const std::string temp_file_1 = createTempFile(dir, file_name_1, NEWLINE_FILE + '\n');
    const std::string temp_file_2 = createTempFile(dir, file_name_2, NEWLINE_FILE + '\n');

    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Multiple file");
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::BaseDirectory.getName(), dir);
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::LookupFrequency.getName(), "0 sec");
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), ".*\\.txt");
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), state_file.str());
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

    std::ofstream newstatefile;
    newstatefile.open(statefile);
    newstatefile << "FILENAME=" << file_name_1 << std::endl;
    newstatefile << "POSITION." << file_name_1 << "=14" << std::endl;
    newstatefile << "CURRENT." << file_name_1 << "=" << temp_file_1 << std::endl;
    newstatefile << "FILENAME=" << file_name_2 << std::endl;
    newstatefile << "POSITION." << file_name_2 << "=15" << std::endl;
    newstatefile << "CURRENT." << file_name_2 << "=" << temp_file_2 << std::endl;
    newstatefile.close();

    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains(file_name_1.substr(0, file_name_1.rfind('.')) + ".14-34.txt"));
    REQUIRE(LogTestController::getInstance().contains(file_name_2.substr(0, file_name_2.rfind('.')) + ".15-34.txt"));

    std::unordered_map<std::string, std::string> state;
    REQUIRE(plan->getStateManagerProvider()->getCoreComponentStateManager(*tailfile)->get(state));

    std::string filePath1, filePath2, fileName1, fileName2;
    REQUIRE(utils::file::PathUtils::getFileNameAndPath(temp_file_1, filePath1, fileName1));
    REQUIRE(utils::file::PathUtils::getFileNameAndPath(temp_file_2, filePath2, fileName2));
    std::unordered_map<std::string, std::string> expected_state{{"file.0.name", fileName1},
                                                                {"file.0.position", "35"},
                                                                {"file.0.current", temp_file_1},
                                                                {"file.0.checksum", "1404369522"},
                                                                {"file.1.name", fileName2},
                                                                {"file.1.position", "35"},
                                                                {"file.1.current", temp_file_2},
                                                                {"file.1.checksum", "2289158555"}};
    for (const auto& key_value_pair : expected_state) {
      const auto it = state.find(key_value_pair.first);
      REQUIRE(it != state.end());
      REQUIRE(it->second == key_value_pair.second);
    }
    REQUIRE(state.find("file.0.last_read_time") != state.end());
    REQUIRE(state.find("file.1.last_read_time") != state.end());
  }
}

TEST_CASE("TailFile picks up the new File to Tail if it is changed between runs", "[state]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tail_file = plan->addProcessor("TailFile", "tail_file");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  std::shared_ptr<core::Processor> log_attribute = plan->addProcessor("LogAttribute", "log_attribute", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog.getName(), "0");

  char format[] = "/tmp/gt.XXXXXX";
  std::string directory = testController.createTempDirectory(format);

  std::string first_test_file = createTempFile(directory, "first.log", "my first log line\n");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), first_test_file);
  testController.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:first.0-17.log"));

  SECTION("The new file gets picked up") {
    std::string second_test_file = createTempFile(directory, "second.log", "my second log line\n");
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), second_test_file);
    plan->reset(true);  // clear the memory, but keep the state file
    LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);
    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:second.0-18.log"));
  }

  SECTION("The old file will no longer be tailed") {
    appendTempFile(directory, "first.log", "add some more stuff\n");
    std::string second_test_file = createTempFile(directory, "second.log", "");
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), second_test_file);
    plan->reset(true);  // clear the memory, but keep the state file
    LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);
    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 0 flow files"));
  }
}

TEST_CASE("TailFile picks up the new File to Tail if it is changed between runs (multiple file mode)", "[state][multiple_file]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  char format[] = "/tmp/gt.XXXXXX";
  std::string directory = testController.createTempDirectory(format);

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tail_file = plan->addProcessor("TailFile", "tail_file");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Multiple file");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::BaseDirectory.getName(), directory);
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::LookupFrequency.getName(), "0 sec");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), "first\\..*\\.log");

  std::shared_ptr<core::Processor> log_attribute = plan->addProcessor("LogAttribute", "log_attribute", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog.getName(), "0");

  createTempFile(directory, "first.fruit.log", "apple\n");
  createTempFile(directory, "second.fruit.log", "orange\n");
  createTempFile(directory, "first.animal.log", "hippopotamus\n");
  testController.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:first.fruit.0-5.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:first.animal.0-12.log"));

  appendTempFile(directory, "first.fruit.log", "banana\n");
  appendTempFile(directory, "first.animal.log", "hedgehog\n");

  SECTION("If a file no longer matches the new regex, then we stop tailing it") {
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), "first\\.f.*\\.log");
    plan->reset(true);  // clear the memory, but keep the state file
    LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);
    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:first.fruit.6-12.log"));
  }

  SECTION("If a new file matches the new regex, we start tailing it") {
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), ".*\\.fruit\\.log");
    plan->reset(true);  // clear the memory, but keep the state file
    LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);
    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow file"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:first.fruit.6-12.log"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:second.fruit.0-6.log"));
  }
}

TEST_CASE("TailFile finds the single input file in both Single and Multiple mode", "[simple]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "");

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::stringstream temp_file;
  temp_file << dir << utils::file::FileUtils::get_separator() << TMP_FILE;
  std::ofstream tmpfile;
  tmpfile.open(temp_file.str(), std::ios::out | std::ios::binary);
  tmpfile << NEWLINE_FILE;
  tmpfile.close();

  SECTION("Single") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  }

  SECTION("Multiple") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), "minifi-.*\\.txt");
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Multiple file");
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::BaseDirectory.getName(), dir);
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::LookupFrequency.getName(), "0 sec");
  }

  testController.runSession(plan, false);
  auto records = plan->getProvenanceRecords();
  REQUIRE(records.size() == 2);

  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
  REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.size()) + " Offset:0"));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile picks up new files created between runs", "[multiple_file]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfile");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Multiple file");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::BaseDirectory.getName(), dir);
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::LookupFrequency.getName(), "0 sec");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), ".*\\.log");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  std::shared_ptr<core::Processor> logattribute = plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);
  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog.getName(), "0");

  createTempFile(dir, "application.log", "line1\nline2\n");

  testController.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));

  createTempFile(dir, "another.log", "some more content\n");

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  testController.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile can handle input files getting removed", "[multiple_file]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfile");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Multiple file");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::BaseDirectory.getName(), dir);
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::LookupFrequency.getName(), "0 sec");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), ".*\\.log");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  std::shared_ptr<core::Processor> logattribute = plan->addProcessor("LogAttribute", "logattribute",
                                                                     core::Relationship("success", "description"),
                                                                     true);
  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog.getName(), "0");

  createTempFile(dir, "one.log", "line one\n");
  createTempFile(dir, "two.log", "some stuff\n");

  testController.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  appendTempFile(dir, "one.log", "line two\nline three\nline four\n");
  removeFile(dir, "two.log");

  testController.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile processes a very long line correctly", "[simple]") {
  std::string line1("foo\n");
  std::string line2(8050, 0);
  std::mt19937 gen(std::random_device{}());  // NOLINT (linter wants a space before '{')
  std::generate_n(line2.begin(), line2.size() - 1, [&]() -> char {
    return 32 + gen() % (127 - 32);
  });
  line2.back() = '\n';
  std::string line3("bar\n");
  std::string line4("buzz");

  TestController testController;
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::stringstream temp_file;
  temp_file << dir << utils::file::FileUtils::get_separator() << TMP_FILE;
  std::ofstream tmpfile;
  tmpfile.open(temp_file.str(), std::ios::out | std::ios::binary);
  tmpfile << line1 << line2 << line3 << line4;
  tmpfile.close();

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  std::shared_ptr<core::Processor> log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");
  plan->setProperty(log_attr, processors::LogAttribute::LogPayload.getName(), "true");
  plan->setProperty(log_attr, processors::LogAttribute::HexencodePayload.getName(), "true");

  uint32_t line_length = 0U;
  SECTION("with line length 80") {
    line_length = 80U;
  }
  SECTION("with line length 200") {
    line_length = 200U;
    plan->setProperty(log_attr, processors::LogAttribute::MaxPayloadLineLength.getName(), "200");
  }
  SECTION("with line length 0") {
    line_length = 0U;
    plan->setProperty(log_attr, processors::LogAttribute::MaxPayloadLineLength.getName(), "0");
  }
  SECTION("with line length 16") {
    line_length = 16U;
    plan->setProperty(log_attr, processors::LogAttribute::MaxPayloadLineLength.getName(), "16");
  }

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));
  REQUIRE(LogTestController::getInstance().contains(utils::StringUtils::to_hex(line1)));
  auto line2_hex = utils::StringUtils::to_hex(line2);
  if (line_length == 0U) {
    REQUIRE(LogTestController::getInstance().contains(line2_hex));
  } else {
    std::stringstream line2_hex_lines;
    for (size_t i = 0; i < line2_hex.size(); i += line_length) {
      line2_hex_lines << line2_hex.substr(i, line_length) << '\n';
    }
    REQUIRE(LogTestController::getInstance().contains(line2_hex_lines.str()));
  }
  REQUIRE(LogTestController::getInstance().contains(utils::StringUtils::to_hex(line3)));
  REQUIRE(false == LogTestController::getInstance().contains(utils::StringUtils::to_hex(line4), std::chrono::seconds(0)));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile processes a long line followed by multiple newlines correctly", "[simple][edge_case]") {
  // Test having two delimiters on the buffer boundary
  std::string line1(4098, '\n');
  std::mt19937 gen(std::random_device { }());
  std::generate_n(line1.begin(), 4095, [&]() -> char {
  return 32 + gen() % (127 - 32);
  });
  std::string line2("foo\n");
  std::string line3("bar\n");
  std::string line4("buzz");

  // Create and write to the test file

  TestController testController;
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto id = tailfile->getUUIDStr();

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::stringstream temp_file;
  temp_file << dir << utils::file::FileUtils::get_separator() << TMP_FILE;
  std::ofstream tmpfile;
  tmpfile.open(temp_file.str(), std::ios::out | std::ios::binary);
  tmpfile << line1 << line2 << line3 << line4;
  tmpfile.close();

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  std::shared_ptr<core::Processor> log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");
  plan->setProperty(log_attr, processors::LogAttribute::LogPayload.getName(), "true");
  plan->setProperty(log_attr, processors::LogAttribute::HexencodePayload.getName(), "true");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged 5 flow files"));
  auto line1_hex = utils::StringUtils::to_hex(line1.substr(0, 4096));
  std::stringstream line1_hex_lines;
  for (size_t i = 0; i < line1_hex.size(); i += 80) {
    line1_hex_lines << line1_hex.substr(i, 80) << '\n';
  }
  REQUIRE(LogTestController::getInstance().contains(line1_hex_lines.str()));
  REQUIRE(LogTestController::getInstance().contains(utils::StringUtils::to_hex(line2)));
  REQUIRE(LogTestController::getInstance().contains(utils::StringUtils::to_hex(line3)));
  REQUIRE(false == LogTestController::getInstance().contains(utils::StringUtils::to_hex(line4), std::chrono::seconds(0)));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile onSchedule throws if file(s) to tail cannot be determined", "[configuration]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::TailFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");

  SECTION("Single file mode by default") {
    SECTION("No FileName") {
    }

    SECTION("FileName does not contain the path") {
      plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), "minifi-log.txt");
    }
  }

  SECTION("Explicit Single file mode") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Single file");

    SECTION("No FileName") {
    }

    SECTION("FileName does not contain the path") {
      plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), "minifi-log.txt");
    }
  }

  SECTION("Multiple file mode") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Multiple file");

    SECTION("No FileName and no BaseDirectory") {
    }

    SECTION("No BaseDirectory") {
      plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), "minifi-.*\\.txt");
    }
  }

  REQUIRE_THROWS(plan->runNextProcessor());
}

TEST_CASE("TailFile onSchedule throws in Multiple mode if the Base Directory does not exist", "[configuration][multiple_file]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::TailFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");
  plan->setProperty(tailfile, processors::TailFile::TailMode.getName(), "Multiple file");
  plan->setProperty(tailfile, processors::TailFile::FileName.getName(), ".*\\.log");

  SECTION("No Base Directory is set") {
    REQUIRE_THROWS(plan->runNextProcessor());
  }

  SECTION("Base Directory is set, but does not exist") {
    std::string nonexistent_file_name{"/no-such-directory/688b01d0-9e5f-11ea-820d-f338c34d39a1/31d1a81a-9e5f-11ea-a77b-8b27d514a452"};
    plan->setProperty(tailfile, processors::TailFile::BaseDirectory.getName(), nonexistent_file_name);
    REQUIRE_THROWS(plan->runNextProcessor());
  }

  SECTION("Base Directory is set and it exists") {
    char format[] = "/tmp/gt.XXXXXX";
    std::string directory = testController.createTempDirectory(format);

    plan->setProperty(tailfile, processors::TailFile::BaseDirectory.getName(), directory);
    plan->setProperty(tailfile, processors::TailFile::LookupFrequency.getName(), "0 sec");
    REQUIRE_NOTHROW(plan->runNextProcessor());
  }
}

TEST_CASE("TailFile finds and finishes the renamed file and continues with the new log file", "[rotation]") {
  TestController testController;

  const char DELIM = ',';
  size_t expected_pieces = std::count(NEWLINE_FILE.begin(), NEWLINE_FILE.end(), DELIM);  // The last piece is left as considered unfinished

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  auto plan = testController.createPlan();

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::string in_file = dir + utils::file::FileUtils::get_separator() + "testfifo.txt";

  std::ofstream in_file_stream(in_file, std::ios::out | std::ios::binary);
  in_file_stream << NEWLINE_FILE;
  in_file_stream.flush();

  // Build MiNiFi processing graph
  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::Delimiter.getName(), std::string(1, DELIM));

  SECTION("single") {
    plan->setProperty(tail_file, processors::TailFile::FileName.getName(), in_file);
  }
  SECTION("Multiple") {
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), "testfifo.txt");
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Multiple file");
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::BaseDirectory.getName(), dir);
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::LookupFrequency.getName(), "0 sec");
  }

  auto log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");
  plan->setProperty(log_attr, processors::LogAttribute::LogPayload.getName(), "true");
  // Log as many FFs as it can to make sure exactly the expected amount is produced

  plan->runNextProcessor();  // Tail
  plan->runNextProcessor();  // Log

  REQUIRE(LogTestController::getInstance().contains(std::string("Logged ") + std::to_string(expected_pieces) + " flow files"));

  std::this_thread::sleep_for(std::chrono::milliseconds(100));  // make sure the new file gets newer modification time

  in_file_stream << DELIM;
  in_file_stream.close();

  std::string rotated_file = (in_file + ".1");

  REQUIRE(rename(in_file.c_str(), rotated_file.c_str()) == 0);

  std::ofstream new_in_file_stream(in_file, std::ios::out | std::ios::binary);
  new_in_file_stream << "five" << DELIM << "six" << DELIM;
  new_in_file_stream.close();

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  plan->runNextProcessor();  // Tail
  plan->runNextProcessor();  // Log

  // Find the last flow file in the rotated file, and then pick up the new file
  REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:testfifo.txt.28-34.1"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:testfifo.0-4.txt"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:testfifo.5-8.txt"));
}

TEST_CASE("TailFile finds and finishes multiple rotated files and continues with the new log file", "[rotation]") {
  TestController testController;

  const char DELIM = ':';

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  auto plan = testController.createPlan();

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::string test_file = dir + utils::file::FileUtils::get_separator() + "fruits.log";

  std::ofstream test_file_stream_0(test_file, std::ios::binary);
  test_file_stream_0 << "Apple" << DELIM << "Orange" << DELIM;
  test_file_stream_0.flush();

  // Build MiNiFi processing graph
  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::Delimiter.getName(), std::string(1, DELIM));
  plan->setProperty(tail_file, processors::TailFile::FileName.getName(), test_file);
  auto log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruits.0-5.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruits.6-12.log"));

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  test_file_stream_0 << "Pear" << DELIM;
  test_file_stream_0.close();

  std::string first_rotated_file = dir + utils::file::FileUtils::get_separator() + "fruits.0.log";
  REQUIRE(rename(test_file.c_str(), first_rotated_file.c_str()) == 0);

  std::ofstream test_file_stream_1(test_file, std::ios::binary);
  test_file_stream_1 << "Pineapple" << DELIM << "Kiwi" << DELIM;
  test_file_stream_1.close();

  std::string second_rotated_file = dir + utils::file::FileUtils::get_separator() + "fruits.1.log";
  REQUIRE(rename(test_file.c_str(), second_rotated_file.c_str()) == 0);

  std::ofstream test_file_stream_2(test_file, std::ios::binary);
  test_file_stream_2 << "Apricot" << DELIM;
  test_file_stream_2.close();

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged 4 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruits.0.13-17.log"));   // Pear
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruits.1.0-9.log"));     // Pineapple
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruits.1.10-14.log"));   // Kiwi
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruits.0-7.log"));       // Apricot
}

TEST_CASE("TailFile ignores old rotated files", "[rotation]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  char format[] = "/tmp/gt.XXXXXX";
  const std::string dir = testController.createTempDirectory(format);

  std::string log_file_name = dir + utils::file::FileUtils::get_separator() + "test.log";

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfile");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), log_file_name);
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  std::shared_ptr<core::Processor> logattribute = plan->addProcessor("LogAttribute", "logattribute",
                                                                     core::Relationship("success", "description"),
                                                                     true);
  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog.getName(), "0");

  createTempFile(dir, "test.2019-08-20", "line1\nline2\nline3\nline4\n");   // very old rotated file
  std::this_thread::sleep_for(std::chrono::seconds(1));

  createTempFile(dir, "test.log", "line5\nline6\nline7\n");

  testController.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));
  REQUIRE(!LogTestController::getInstance().contains("key:filename value:test.2019-08-20"));

  std::string rotated_log_file_name = dir + utils::file::FileUtils::get_separator() + "test.2020-05-18";
  REQUIRE(rename(log_file_name.c_str(), rotated_log_file_name.c_str()) == 0);

  createTempFile(dir, "test.log", "line8\nline9\n");

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  testController.runSession(plan, true);
  REQUIRE(!LogTestController::getInstance().contains("key:filename value:test.2019-08-20"));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile rotation works with multiple input files", "[rotation][multiple_file]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  auto plan = testController.createPlan();

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  createTempFile(dir, "fruit.log", "apple\npear\nbanana\n");
  createTempFile(dir, "animal.log", "bear\ngiraffe\n");
  createTempFile(dir, "color.log", "red\nblue\nyellow\npurple\n");

  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::Delimiter.getName(), "\n");
  plan->setProperty(tail_file, processors::TailFile::TailMode.getName(), "Multiple file");
  plan->setProperty(tail_file, processors::TailFile::FileName.getName(), ".*\\.log");
  plan->setProperty(tail_file, processors::TailFile::BaseDirectory.getName(), dir);
  plan->setProperty(tail_file, processors::TailFile::LookupFrequency.getName(), "0 sec");

  auto log_attribute = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, processors::LogAttribute::FlowFilesToLog.getName(), "0");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged " + std::to_string(3 + 2 + 4) + " flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruit.0-5.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruit.6-10.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruit.11-17.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:animal.0-4.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:animal.5-12.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.0-3.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.4-8.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.9-15.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.16-22.log"));

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  appendTempFile(dir, "fruit.log", "orange\n");
  appendTempFile(dir, "animal.log", "axolotl\n");
  appendTempFile(dir, "color.log", "aquamarine\n");

  renameTempFile(dir, "fruit.log", "fruit.0");
  renameTempFile(dir, "animal.log", "animal.0");

  createTempFile(dir, "fruit.log", "peach\n");
  createTempFile(dir, "animal.log", "dinosaur\n");
  appendTempFile(dir, "color.log", "turquoise\n");

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged 6 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruit.18-24.0"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruit.0-5.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:animal.13-20.0"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:animal.0-8.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.23-33.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.34-43.log"));
}

TEST_CASE("TailFile handles the Rolling Filename Pattern property correctly", "[rotation]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  auto plan = testController.createPlan();

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::string test_file = createTempFile(dir, "test.log", "some stuff\n");

  // Build MiNiFi processing graph
  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::Delimiter.getName(), "\n");
  plan->setProperty(tail_file, processors::TailFile::FileName.getName(), test_file);

  std::vector<std::string> expected_log_lines;

  SECTION("If no pattern is set, we use the default, which is ${filename}.*, so the unrelated file will be picked up") {
    expected_log_lines = std::vector<std::string>{"Logged 2 flow files",
                                                  "test.rolled.11-24.log",
                                                  "test.0-15.txt"};
  }

  SECTION("If a pattern is set to exclude the unrelated file, we no longer pick it up") {
    plan->setProperty(tail_file, processors::TailFile::RollingFilenamePattern.getName(), "${filename}.*.log");

    expected_log_lines = std::vector<std::string>{"Logged 1 flow file",
                                                  "test.rolled.11-24.log"};
  }

  SECTION("We can also set the pattern to not include the file name") {
    plan->setProperty(tail_file, processors::TailFile::RollingFilenamePattern.getName(), "other_roll??.log");

    expected_log_lines = std::vector<std::string>{"Logged 1 flow file",
                                                  "other_rolled.11-24.log"};
  }

  auto log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:test.0-10.log"));

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  appendTempFile(dir, "test.log", "one more line\n");
  renameTempFile(dir, "test.log", "test.rolled.log");
  createTempFile(dir, "test.txt", "unrelated stuff\n");
  createTempFile(dir, "other_rolled.log", "some stuff\none more line\n");  // same contents as test.rolled.log

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  testController.runSession(plan, true);

  for (const auto &log_line : expected_log_lines) {
    REQUIRE(LogTestController::getInstance().contains(log_line));
  }
}

TEST_CASE("TailFile finds and finishes the renamed file and continues with the new log file after a restart", "[rotation][restart]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  char log_dir_format[] = "/tmp/gt.XXXXXX";
  auto log_dir = testController.createTempDirectory(log_dir_format);

  std::string test_file_1 = createTempFile(log_dir, "test.1", "line one\nline two\nline three\n");  // old rotated file
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::string test_file = createTempFile(log_dir, "test.log", "line four\nline five\nline six\n");  // current log file

  char state_dir_format[] = "/tmp/gt.XXXXXX";
  auto state_dir = testController.createTempDirectory(state_dir_format);

  utils::Identifier tail_file_uuid = utils::IdGenerator::getIdGenerator()->generate();
  const core::Relationship success_relationship{"success", "everything is fine"};

  {
    auto test_plan = testController.createPlan(nullptr, state_dir.c_str());
    auto tail_file = test_plan->addProcessor("TailFile", tail_file_uuid, "Tail", {success_relationship});
    test_plan->setProperty(tail_file, processors::TailFile::FileName.getName(), test_file);
    auto log_attr = test_plan->addProcessor("LogAttribute", "Log", success_relationship, true);
    test_plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");
    test_plan->setProperty(log_attr, processors::LogAttribute::LogPayload.getName(), "true");

    testController.runSession(test_plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));
  }

  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  appendTempFile(log_dir, "test.log", "line seven\n");
  renameTempFile(log_dir, "test.1", "test.2");
  renameTempFile(log_dir, "test.log", "test.1");
  createTempFile(log_dir, "test.log", "line eight is the last line\n");

  {
    auto test_plan = testController.createPlan(nullptr, state_dir.c_str());
    auto tail_file = test_plan->addProcessor("TailFile", tail_file_uuid, "Tail", {success_relationship});
    test_plan->setProperty(tail_file, processors::TailFile::FileName.getName(), test_file);
    auto log_attr = test_plan->addProcessor("LogAttribute", "Log", success_relationship, true);
    test_plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");
    test_plan->setProperty(log_attr, processors::LogAttribute::LogPayload.getName(), "true");

    testController.runSession(test_plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:test.29-39.1"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:test.0-27.log"));
  }
}

TEST_CASE("TailFile yields if no work is done", "[yield]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  char format[] = "/tmp/gt.XXXXXX";
  auto temp_directory = testController.createTempDirectory(format);

  auto plan = testController.createPlan();

  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::Delimiter.getName(), "\n");
  plan->setProperty(tail_file, processors::TailFile::TailMode.getName(), "Multiple file");
  plan->setProperty(tail_file, processors::TailFile::FileName.getName(), ".*\\.log");
  plan->setProperty(tail_file, processors::TailFile::BaseDirectory.getName(), temp_directory);
  plan->setProperty(tail_file, processors::TailFile::LookupFrequency.getName(), "0 sec");

  SECTION("Empty log file => yield") {
    createTempFile(temp_directory, "first.log", "");

    testController.runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() > 0);

    SECTION("No logging happened between onTrigger calls => yield") {
      plan->reset();
      tail_file->clearYield();

      testController.runSession(plan, true);

      REQUIRE(tail_file->getYieldTime() > 0);
    }

    SECTION("Some logging happened between onTrigger calls => don't yield") {
      plan->reset();
      tail_file->clearYield();

      appendTempFile(temp_directory, "first.log", "stuff stuff\nand stuff\n");

      testController.runSession(plan, true);

      REQUIRE(tail_file->getYieldTime() == 0);
    }
  }

  SECTION("Non-empty log file => don't yield") {
    createTempFile(temp_directory, "second.log", "some content\n");

    testController.runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() == 0);

    SECTION("No logging happened between onTrigger calls => yield") {
      plan->reset();
      tail_file->clearYield();

      testController.runSession(plan, true);

      REQUIRE(tail_file->getYieldTime() > 0);
    }

    SECTION("Some logging happened between onTrigger calls => don't yield") {
      plan->reset();
      tail_file->clearYield();

      appendTempFile(temp_directory, "second.log", "stuff stuff\nand stuff\n");

      testController.runSession(plan, true);

      REQUIRE(tail_file->getYieldTime() == 0);
    }
  }
}

TEST_CASE("TailFile yields if no work is done on any files", "[yield][multiple_file]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  char format[] = "/tmp/gt.XXXXXX";
  auto temp_directory = testController.createTempDirectory(format);

  auto plan = testController.createPlan();

  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::Delimiter.getName(), "\n");
  plan->setProperty(tail_file, processors::TailFile::TailMode.getName(), "Multiple file");
  plan->setProperty(tail_file, processors::TailFile::FileName.getName(), ".*\\.log");
  plan->setProperty(tail_file, processors::TailFile::BaseDirectory.getName(), temp_directory);
  plan->setProperty(tail_file, processors::TailFile::LookupFrequency.getName(), "0 sec");

  createTempFile(temp_directory, "first.log", "stuff\n");
  createTempFile(temp_directory, "second.log", "different stuff\n");
  createTempFile(temp_directory, "third.log", "stuff stuff\n");

  testController.runSession(plan, true);
  plan->reset();
  tail_file->clearYield();

  SECTION("No file changed => yield") {
    testController.runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() > 0);
  }

  SECTION("One file changed => don't yield") {
    SECTION("first") { appendTempFile(temp_directory, "first.log", "more stuff\n"); }
    SECTION("second") { appendTempFile(temp_directory, "second.log", "more stuff\n"); }
    SECTION("third") { appendTempFile(temp_directory, "third.log", "more stuff\n"); }

    testController.runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() == 0);
  }

  SECTION("More than one file changed => don't yield") {
    SECTION("first and third") {
      appendTempFile(temp_directory, "first.log", "more stuff\n");
      appendTempFile(temp_directory, "third.log", "more stuff\n");
    }
    SECTION("all of them") {
      appendTempFile(temp_directory, "first.log", "more stuff\n");
      appendTempFile(temp_directory, "second.log", "more stuff\n");
      appendTempFile(temp_directory, "third.log", "more stuff\n");
    }

    testController.runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() == 0);
  }
}

TEST_CASE("TailFile doesn't yield if work was done on rotated files only", "[yield][rotation]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  char format[] = "/tmp/gt.XXXXXX";
  auto temp_directory = testController.createTempDirectory(format);
  std::string full_file_name = createTempFile(temp_directory, "test.log", "stuff\n");

  auto plan = testController.createPlan();

  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::Delimiter.getName(), "\n");
  plan->setProperty(tail_file, processors::TailFile::FileName.getName(), full_file_name);

  testController.runSession(plan, true);

  plan->reset();
  tail_file->clearYield();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  SECTION("File rotated but not written => yield") {
    renameTempFile(temp_directory, "test.log", "test.1");

    SECTION("Don't create empty new log file") {
    }
    SECTION("Create empty new log file") {
      createTempFile(temp_directory, "test.log", "");
    }

    testController.runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() > 0);
  }

  SECTION("File rotated and new stuff is added => don't yield") {
    SECTION("New content before rotation") {
      appendTempFile(temp_directory, "test.log", "more stuff\n");
    }

    renameTempFile(temp_directory, "test.log", "test.1");

    SECTION("New content after rotation") {
      createTempFile(temp_directory, "test.log", "even more stuff\n");
    }

    testController.runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() == 0);
  }
}

TEST_CASE("TailFile handles the Delimiter setting correctly", "[delimiter]") {
  std::vector<std::pair<std::string, std::string>> test_cases = {
      // first = value of Delimiter in the config
      // second = the expected delimiter char which will be used
      {"", ""}, {",", ","}, {"\t", "\t"}, {"\\t", "\t"}, {"\n", "\n"}, {"\\n", "\n"}, {"\\", "\\"}, {"\\\\", "\\"}};
  for (const auto &test_case : test_cases) {
    TestController testController;

    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setTrace<processors::TailFile>();
    LogTestController::getInstance().setTrace<processors::LogAttribute>();

    char format[] = "/tmp/gt.XXXXXX";
    auto temp_directory = testController.createTempDirectory(format);

    std::string delimiter = test_case.second;
    std::string full_file_name = createTempFile(temp_directory, "test.log", "one" + delimiter + "two" + delimiter);

    auto plan = testController.createPlan();

    auto tail_file = plan->addProcessor("TailFile", "Tail");
    plan->setProperty(tail_file, processors::TailFile::Delimiter.getName(), test_case.first);
    plan->setProperty(tail_file, processors::TailFile::FileName.getName(), full_file_name);

    auto log_attribute = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
    plan->setProperty(log_attribute, processors::LogAttribute::FlowFilesToLog.getName(), "0");

    testController.runSession(plan, true);

    if (delimiter.empty()) {
      REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
      REQUIRE(LogTestController::getInstance().contains("key:filename value:test.0-5.log"));
    } else {
      REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
      REQUIRE(LogTestController::getInstance().contains("key:filename value:test.0-3.log"));
      REQUIRE(LogTestController::getInstance().contains("key:filename value:test.4-7.log"));
    }
  }
}

TEST_CASE("TailFile handles Unix/Windows line endings correctly", "[simple]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  char format[] = "/tmp/gt.XXXXXX";
  auto temp_directory = testController.createTempDirectory(format);
  std::string full_file_name = createTempFile(temp_directory, "test.log", "line1\nline two\n", std::ios::out);  // write in text mode

  auto plan = testController.createPlan();

  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::FileName.getName(), full_file_name);

  auto log_attribute = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, processors::LogAttribute::FlowFilesToLog.getName(), "0");

  testController.runSession(plan, true);

#ifdef WIN32
  std::size_t line_ending_size = 2;
#else
  std::size_t line_ending_size = 1;
#endif
  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(5 + line_ending_size) + " Offset:0"));
  REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(8 + line_ending_size) + " Offset:0"));
}

TEST_CASE("TailFile can tail all files in a directory recursively", "[multiple]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  char format[] = "/tmp/gt.XXXXXX";
  std::string base_directory = testController.createTempDirectory(format);
  std::string directory1 = base_directory + utils::file::FileUtils::get_separator() + "one";
  utils::file::FileUtils::create_dir(directory1);
  std::string directory11 = directory1 + utils::file::FileUtils::get_separator() + "one_child";
  utils::file::FileUtils::create_dir(directory11);
  std::string directory2 = base_directory + utils::file::FileUtils::get_separator() + "two";
  utils::file::FileUtils::create_dir(directory2);

  createTempFile(base_directory, "test.orange.log", "orange juice\n");
  createTempFile(directory1, "test.blue.log", "blue\n");
  createTempFile(directory1, "test.orange.log", "orange autumn leaves\n");
  createTempFile(directory11, "test.camel.log", "camel\n");
  createTempFile(directory2, "test.triangle.log", "triangle\n");

  auto plan = testController.createPlan();

  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::TailMode.getName(), "Multiple file");
  plan->setProperty(tail_file, processors::TailFile::BaseDirectory.getName(), base_directory);
  plan->setProperty(tail_file, processors::TailFile::LookupFrequency.getName(), "0 sec");
  plan->setProperty(tail_file, processors::TailFile::FileName.getName(), ".*\\.log");

  auto log_attribute = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, processors::LogAttribute::FlowFilesToLog.getName(), "0");
  plan->setProperty(log_attribute, processors::LogAttribute::LogPayload.getName(), "true");

  SECTION("Recursive lookup not set => defaults to false") {
    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
  }

  SECTION("Recursive lookup set to false") {
    plan->setProperty(tail_file, processors::TailFile::RecursiveLookup.getName(), "false");
    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
  }

  SECTION("Recursive lookup set to true") {
    plan->setProperty(tail_file, processors::TailFile::RecursiveLookup.getName(), "true");
    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 5 flow files"));
  }
}

TEST_CASE("TailFile interprets the lookup frequency property correctly", "[multiple]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  char format[] = "/tmp/gt.XXXXXX";
  std::string directory = testController.createTempDirectory(format);

  createTempFile(directory, "test.red.log", "cherry\n");

  auto plan = testController.createPlan();

  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::TailMode.getName(), "Multiple file");
  plan->setProperty(tail_file, processors::TailFile::BaseDirectory.getName(), directory);
  plan->setProperty(tail_file, processors::TailFile::FileName.getName(), ".*\\.log");

  auto log_attribute = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, processors::LogAttribute::FlowFilesToLog.getName(), "0");

  testController.runSession(plan, true);

  SECTION("Lookup frequency not set => defaults to 10 minutes") {
    std::shared_ptr<processors::TailFile> tail_file_processor = std::dynamic_pointer_cast<processors::TailFile>(tail_file);
    REQUIRE(tail_file_processor);
    REQUIRE(tail_file_processor->getLookupFrequency() == std::chrono::minutes{10});
  }

  SECTION("Lookup frequency set to zero => new files are picked up immediately") {
    plan->setProperty(tail_file, processors::TailFile::LookupFrequency.getName(), "0 sec");

    plan->reset(true);
    LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

    createTempFile(directory, "test.blue.log", "sky\n");
    createTempFile(directory, "test.green.log", "grass\n");

    testController.runSession(plan, true);

    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  }

  SECTION("Lookup frequency set to 100 ms => new files are only picked up after 100 ms") {
    plan->setProperty(tail_file, processors::TailFile::LookupFrequency.getName(), "100 ms");

    plan->reset(true);
    LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

    createTempFile(directory, "test.blue.log", "sky\n");
    createTempFile(directory, "test.green.log", "grass\n");

    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 0 flow files"));

    plan->reset(false);
    LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  }
}
