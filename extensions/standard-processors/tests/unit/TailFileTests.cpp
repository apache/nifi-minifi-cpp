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

#include <stdio.h>
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
TEST_CASE("TailFileWithDelimiter", "[tailfiletest2]") {
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
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), state_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  testController.runSession(plan, false);
  auto records = plan->getProvenanceRecords();
  REQUIRE(records.size() == 2);

  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
  REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.find_first_of('\n')) + " Offset:0"));

  LogTestController::getInstance().reset();

  // Delete the test and state file.

  remove(std::string(std::string(STATE_FILE) + "." + id).c_str());
}

TEST_CASE("TestNewContent", "[tailFileWithDelimiterState]") {
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

  std::stringstream state_file;
  state_file << dir << utils::file::FileUtils::get_separator() << STATE_FILE;

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), state_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("minifi-tmpfile.0-13.txt"));

  plan->reset(true);  // start a new but with state file

  std::ofstream appendStream;
  appendStream.open(temp_file.str(), std::ios_base::app | std::ios_base::binary);
  appendStream << std::endl;
  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("position 14"));
  REQUIRE(LogTestController::getInstance().contains("minifi-tmpfile.14-34.txt"));

  LogTestController::getInstance().reset();

  // Delete the test and state file.

  remove(std::string(std::string(STATE_FILE) + "." + id).c_str());
}

TEST_CASE("TestDeleteState", "[tailFileWithDelimiterState]") {
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

  std::stringstream state_file;
  state_file << dir << utils::file::FileUtils::get_separator() << STATE_FILE;

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), state_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");
  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("minifi-tmpfile.0-13.txt"));

  plan->reset(true);  // start a new but with state file
  remove(std::string(state_file.str() + "." + id).c_str());

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("position 0"));

  // if we lose state we restart
  REQUIRE(LogTestController::getInstance().contains("minifi-tmpfile.0-13.txt"));

  // Delete the test and state file.
}

TEST_CASE("TestChangeState", "[tailFileWithDelimiterState]") {
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

  std::stringstream state_file;
  state_file << dir << utils::file::FileUtils::get_separator() << STATE_FILE;

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), state_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("minifi-tmpfile.0-13.txt"));


  // should stay the same
  for (int i = 0; i < 5; i++) {
    plan->reset(true);  // start a new but with state file

    auto statefile = state_file.str() + "." + id;

    remove(statefile.c_str());

    std::ofstream newstatefile;
    newstatefile.open(statefile);
    newstatefile << "FILENAME=" << temp_file.str() << std::endl;
    newstatefile << "POSITION=14" << std::endl;
    newstatefile.close();

    testController.runSession(plan, true);

    REQUIRE(LogTestController::getInstance().contains("position 14"));

    // if we lose state we restart
    REQUIRE(LogTestController::getInstance().contains("minifi-tmpfile.14-34.txt"));
  }
  for (int i = 14; i < 34; i++) {
    plan->reset(true);  // start a new but with state file

    auto statefile = state_file.str() + "." + id;

    remove(statefile.c_str());

    std::ofstream newstatefile;
    newstatefile.open(statefile);
    newstatefile << "FILENAME=" << temp_file.str() << std::endl;
    newstatefile << "POSITION=" << i << std::endl;
    newstatefile.close();
    testController.runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("position " + std::to_string(i)));
  }

  plan->runCurrentProcessor();
  for (int i = 14; i < 34; i++) {
    REQUIRE(LogTestController::getInstance().contains("minifi-tmpfile." + std::to_string(i) + "-34.txt"));
  }
  // Delete the test and state file.

  remove(std::string(state_file.str() + "." + id).c_str());
}

TEST_CASE("TestInvalidState", "[tailFileWithDelimiterState]") {
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
  tmpfile.open(temp_file.str());
  tmpfile << NEWLINE_FILE;
  tmpfile.close();

  std::ofstream appendStream;
  appendStream.open(temp_file.str(), std::ios_base::app);
  appendStream.write("\n", 1);
  appendStream.close();

  std::stringstream state_file;
  state_file << dir << utils::file::FileUtils::get_separator() << STATE_FILE;

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), state_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  testController.runSession(plan, true);

#ifdef WIN32
  REQUIRE(LogTestController::getInstance().contains("minifi-tmpfile.0-14.txt"));
#else
  REQUIRE(LogTestController::getInstance().contains("minifi-tmpfile.0-13.txt"));
#endif

  plan->reset(true);  // start a new but with state file

  auto statefile = state_file.str() + "." + id;

  remove(statefile.c_str());

  SECTION("No Filename") {
  std::ofstream newstatefile;
  newstatefile.open(statefile);
  newstatefile << "POSITION=14" << std::endl;
  newstatefile.close();
  REQUIRE_THROWS(testController.runSession(plan, true));
  }

  SECTION("Invalid current filename") {
  std::ofstream newstatefile;
  newstatefile.open(statefile);
  newstatefile << "FILENAME=minifi-tmpfile.txt" << std::endl;
  newstatefile << "CURRENT.minifi-tempfile.txt=minifi-tmpfile.txt" << std::endl;
  newstatefile << "POSITION=14" << std::endl;
  newstatefile.close();
  REQUIRE_THROWS(testController.runSession(plan, true));
  }
  SECTION("No current filename and partial path") {
  std::ofstream newstatefile;
  newstatefile.open(statefile);
  newstatefile << "FILENAME=minifi-tmpfile.txt" << std::endl;
  newstatefile << "POSITION=14" << std::endl;
  newstatefile.close();
  REQUIRE_THROWS(testController.runSession(plan, true));
  }

// Delete the test and state file.

  remove(std::string(std::string(STATE_FILE) + "." + id).c_str());
}

TEST_CASE("TailFileWithOutDelimiter", "[tailfiletest2]") {
  // Create and write to the test file

  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
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
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), state_file.str());

  testController.runSession(plan, false);
  auto records = plan->getProvenanceRecords();
  REQUIRE(records.size() == 2);

  testController.runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
  REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.size()) + " Offset:0"));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFileLongWithDelimiter", "[tailfiletest2]") {
  std::string line1("foo");
  std::string line2(8050, 0);
  std::mt19937 gen(std::random_device { }());
  std::generate_n(line2.begin(), line2.size(), [&]() -> char {
    return 32 + gen() % (127 - 32);
  });
  std::string line3("bar");
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
  tmpfile << line1 << "\n" << line2 << "\n" << line3 << "\n" << line4;
  tmpfile.close();

  std::stringstream state_file;
  state_file << dir << utils::file::FileUtils::get_separator() << STATE_FILE;

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), state_file.str());
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

TEST_CASE("TailFileWithDelimiterMultipleDelimiters", "[tailfiletest2]") {
  // Test having two delimiters on the buffer boundary
  std::string line1(4097, '\n');
  std::mt19937 gen(std::random_device { }());
  std::generate_n(line1.begin(), 4095, [&]() -> char {
    return 32 + gen() % (127 - 32);
  });
  std::string line2("foo");
  std::string line3("bar");
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
  tmpfile << line1 << "\n" << line2 << "\n" << line3 << "\n" << line4;
  tmpfile.close();

  std::stringstream state_file;
  state_file << dir << utils::file::FileUtils::get_separator() << STATE_FILE;

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), state_file.str());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  std::shared_ptr<core::Processor> log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");
  plan->setProperty(log_attr, processors::LogAttribute::LogPayload.getName(), "true");
  plan->setProperty(log_attr, processors::LogAttribute::HexencodePayload.getName(), "true");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged 5 flow files"));
  auto line1_hex = utils::StringUtils::to_hex(line1.substr(0, 4095));
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

TEST_CASE("TailWithInvalid", "[tailfiletest2]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto id = tailfile->getUUIDStr();

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  SECTION("No File and No base") {
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Multiple file");
  }

  SECTION("No base") {
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), "minifi-.*\\.txt");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Multiple file");
  }
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), STATE_FILE);

  REQUIRE_THROWS(plan->runNextProcessor());
}

TEST_CASE("TailFileWithRealDelimiterAndRotate", "[tailfiletest2]") {
  TestController testController;

  const char DELIM = ',';
  size_t expected_pieces = std::count(NEWLINE_FILE.begin(), NEWLINE_FILE.end(), DELIM);  // The last piece is left as considered unfinished

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  auto plan = testController.createPlan();

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  // Define test input file
  std::string in_file(dir);
#ifndef WIN32
  in_file.append("/");
#else
  in_file.append("\\");
#endif
  in_file.append("testfifo.txt");

  std::string state_file(dir);
  state_file.append("tailfile.state");

  std::ofstream in_file_stream(in_file);
  in_file_stream << NEWLINE_FILE;
  in_file_stream.flush();

  // Build MiNiFi processing graph
  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::Delimiter.getName(), std::string(1, DELIM));

  SECTION("single") {
  plan->setProperty(
      tail_file,
      processors::TailFile::FileName.getName(), in_file);
  }
  SECTION("Multiple") {
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), "test.*");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Multiple file");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::BaseDirectory.getName(), dir);
  }
  plan->setProperty(tail_file, processors::TailFile::StateFile.getName(), state_file);
  auto log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");
  plan->setProperty(log_attr, processors::LogAttribute::LogPayload.getName(), "true");
  // Log as many FFs as it can to make sure exactly the expected amount is produced

  plan->runNextProcessor();  // Tail
  plan->runNextProcessor();  // Log
  std::cout << " find " << expected_pieces << std::endl;
  REQUIRE(LogTestController::getInstance().contains(std::string("Logged ") + std::to_string(expected_pieces) + " flow files"));

  in_file_stream << DELIM;
  in_file_stream.close();

  std::string rotated_file = (in_file + ".1");

  REQUIRE(rename(in_file.c_str(), rotated_file.c_str()) == 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));  // make sure the new file gets newer modification time

  std::ofstream new_in_file_stream(in_file);
  new_in_file_stream << "five" << DELIM << "six" << DELIM;
  new_in_file_stream.close();

  plan->reset();
  plan->runNextProcessor();  // Tail
  plan->runNextProcessor();  // Log

  // Find the last flow file in the rotated file
  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));

  plan->reset();
  plan->runNextProcessor();  // Tail
  plan->runNextProcessor();  // Log

  // Two new files in the new flow file
  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
}

TEST_CASE("TailFileWithMultileRolledOverFiles", "[tailfiletest2]") {
  TestController testController;

  const char DELIM = ':';

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TailFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  auto plan = testController.createPlan();

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::string state_file(dir);
  state_file.append("tailfile.state");

  // Define test input file
  std::string in_file(dir);
  in_file.append("fruits.txt");

  for (int i = 2; 0 <= i; --i) {
    if (i < 2) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));  // make sure the new file gets newer modification time
    }
    std::ofstream in_file_stream(in_file + (i > 0 ? std::to_string(i) : ""));
    for (int j = 0; j <= i; j++) {
      in_file_stream << "Apple" << DELIM;
    }
    in_file_stream.close();
  }

  // Build MiNiFi processing graph
  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, processors::TailFile::Delimiter.getName(), std::string(1, DELIM));
  plan->setProperty(tail_file, processors::TailFile::FileName.getName(), in_file);
  plan->setProperty(tail_file, processors::TailFile::StateFile.getName(), state_file);
  auto log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");
  // Log as many FFs as it can to make sure exactly the expected amount is produced

  // Each iteration should go through one file and log all flowfiles
  for (int i = 2; 0 <= i; --i) {
    plan->reset();
    plan->runNextProcessor();  // Tail
    plan->runNextProcessor();  // Log

    REQUIRE(LogTestController::getInstance().contains(std::string("Logged ") + std::to_string(i + 1) + " flow files"));
  }

  // Rrite some more data to the source file
  std::ofstream in_file_stream(in_file);
  in_file_stream << "Pear" << DELIM << "Cherry" << DELIM;

  plan->reset();
  plan->runNextProcessor();  // Tail
  plan->runNextProcessor();  // Log

  REQUIRE(LogTestController::getInstance().contains(std::string("Logged 2 flow files")));
}
