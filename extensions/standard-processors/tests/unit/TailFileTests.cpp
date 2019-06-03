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
#include <uuid/uuid.h>
#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <iostream>
#include <set>
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
static const char *TMP_FILE = "/tmp/minifi-tmpfile.txt";
static const char *STATE_FILE = "/tmp/minifi-state-file.txt";

TEST_CASE("TailFileWithDelimiter", "[tailfiletest2]") {
  // Create and write to the test file
  std::ofstream tmpfile;
  tmpfile.open(TMP_FILE);
  tmpfile << NEWLINE_FILE;
  tmpfile.close();

  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), TMP_FILE);
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), STATE_FILE);
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter.getName(), "\n");

  testController.runSession(plan, false);
  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 2);

  LogTestController::getInstance().reset();

  // Delete the test and state file.
  remove(TMP_FILE);
  remove(STATE_FILE);
}

TEST_CASE("TailFileWithOutDelimiter", "[tailfiletest2]") {
  // Create and write to the test file

  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);
  std::stringstream temp_file_ss;
  temp_file_ss << dir << utils::file::FileUtils::get_separator() << "minifi-tmpfile.txt";
  auto temp_file = temp_file_ss.str();
  std::ofstream tmpfile;
  tmpfile.open(temp_file);
  tmpfile << NEWLINE_FILE;
  tmpfile.close();

  SECTION("Single") {
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), temp_file);
}

  SECTION("Multiple") {
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), "minifi-.*\\.txt");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode.getName(), "Multiple file");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::BaseDirectory.getName(), dir);
}
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), STATE_FILE);

  testController.runSession(plan, false);
  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 2);

  testController.runSession(plan, false);

  LogTestController::getInstance().reset();

  // Delete the test and state file.
  remove(STATE_FILE);
}

TEST_CASE("TailWithInvalid", "[tailfiletest2]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

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
  LogTestController::getInstance().setTrace<core::ProcessSession>();

  auto plan = testController.createPlan();

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  // Define test input file
  std::string in_file(dir);
  in_file.append("/testfifo.txt");

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
  LogTestController::getInstance().setTrace<core::ProcessSession>();

  auto plan = testController.createPlan();

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  std::string state_file(dir);
  state_file.append("tailfile.state");

  // Define test input file
  std::string in_file(dir);
  in_file.append("/fruits.txt");

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

/*
 TEST_CASE("TailFileWithDelimiter", "[tailfiletest1]") {
 try {
 // Create and write to the test file
 std::ofstream tmpfile;
 tmpfile.open(TMP_FILE);
 tmpfile << NEWLINE_FILE;
 tmpfile.close();

 TestController testController;
 LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::TailFile>();
 LogTestController::getInstance().setDebug<core::ProcessSession>();
 LogTestController::getInstance().setDebug<core::repository::VolatileContentRepository>();

 std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

 std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::TailFile>("tailfile");
 std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");

 utils::Identifier processoruuid;
 REQUIRE(true == processor->getUUID(processoruuid));
 utils::Identifier logAttributeuuid;
 REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

 std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
 content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
 std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
 connection->addRelationship(core::Relationship("success", "TailFile successful output"));

 // link the connections so that we can test results at the end for this
 connection->setDestination(connection);

 connection->setSourceUUID(processoruuid);

 processor->addConnection(connection);

 std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);

 std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
 core::ProcessContext context(node, controller_services_provider, repo, repo, content_repo);
 context.setProperty(org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");
 context.setProperty(org::apache::nifi::minifi::processors::TailFile::FileName, TMP_FILE);
 context.setProperty(org::apache::nifi::minifi::processors::TailFile::StateFile, STATE_FILE);

 core::ProcessSession session(&context);

 REQUIRE(processor->getName() == "tailfile");

 core::ProcessSessionFactory factory(&context);

 std::shared_ptr<core::FlowFile> record;
 processor->setScheduledState(core::ScheduledState::RUNNING);
 processor->onSchedule(&context, &factory);
 processor->onTrigger(&context, &session);

 provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
 std::set<provenance::ProvenanceEventRecord*> provRecords = reporter->getEvents();
 record = session.get();
 REQUIRE(record == nullptr);
 std::shared_ptr<core::FlowFile> ff = session.get();
 REQUIRE(provRecords.size() == 4);   // 2 creates and 2 modifies for flowfiles

 LogTestController::getInstance().reset();
 } catch (...) {
 }

 // Delete the test and state file.
 std::remove(TMP_FILE);
 std::remove(STATE_FILE);
 }


 TEST_CASE("TailFileWithoutDelimiter", "[tailfiletest2]") {
 try {
 // Create and write to the test file
 std::ofstream tmpfile;
 tmpfile.open(TMP_FILE);
 tmpfile << NEWLINE_FILE;
 tmpfile.close();

 TestController testController;
 LogTestController::getInstance().setInfo<org::apache::nifi::minifi::processors::TailFile>();

 std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

 std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::TailFile>("tailfile");
 std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");

 utils::Identifier processoruuid;
 REQUIRE(true == processor->getUUID(processoruuid));
 utils::Identifier logAttributeuuid;
 REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

 std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
 content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
 std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
 connection->addRelationship(core::Relationship("success", "TailFile successful output"));

 // link the connections so that we can test results at the end for this
 connection->setDestination(connection);
 connection->setSourceUUID(processoruuid);

 processor->addConnection(connection);

 std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);

 std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
 core::ProcessContext context(node, controller_services_provider, repo, repo, content_repo);
 context.setProperty(org::apache::nifi::minifi::processors::TailFile::FileName, TMP_FILE);
 context.setProperty(org::apache::nifi::minifi::processors::TailFile::StateFile, STATE_FILE);

 core::ProcessSession session(&context);

 REQUIRE(processor->getName() == "tailfile");

 core::ProcessSessionFactory factory(&context);

 std::shared_ptr<core::FlowFile> record;
 processor->setScheduledState(core::ScheduledState::RUNNING);
 processor->onSchedule(&context, &factory);
 processor->onTrigger(&context, &session);

 provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
 std::set<provenance::ProvenanceEventRecord*> provRecords = reporter->getEvents();
 record = session.get();
 REQUIRE(record == nullptr);
 std::shared_ptr<core::FlowFile> ff = session.get();
 REQUIRE(provRecords.size() == 2);

 LogTestController::getInstance().reset();
 } catch (...) {
 }

 // Delete the test and state file.
 std::remove(TMP_FILE);
 std::remove(STATE_FILE);
 }
 */
