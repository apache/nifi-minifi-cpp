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

#include <uuid/uuid.h>
#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <set>
#include "FlowController.h"
#include "../TestBase.h"
#include "core/Core.h"
#include "../../include/core/FlowFile.h"
#include "../unit/ProvenanceTestHelper.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "processors/TailFile.h"
#include "processors/LogAttribute.h"
#include <iostream>

static const char *NEWLINE_FILE = ""
    "one,two,three\n"
    "four,five,six, seven";
static const char *TMP_FILE = "/tmp/minifi-tmpfile.txt";
static const char *STATE_FILE = "/tmp/minifi-state-file.txt";

TEST_CASE("TailFileWithDelimiter", "[tailfiletest1]") {
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
    std::remove(TMP_FILE);
    std::remove(STATE_FILE);
}

TEST_CASE("TailFileWithOutDelimiter", "[tailfiletest2]") {
  // Create and write to the test file
      std::ofstream tmpfile;
      tmpfile.open(TMP_FILE);
      tmpfile << NEWLINE_FILE;
      tmpfile.close();

  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> tailfile = plan->addProcessor("TailFile", "tailfileProc");

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName.getName(), TMP_FILE);
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile.getName(), STATE_FILE);


  testController.runSession(plan, false);
  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 2);

  testController.runSession(plan, false);

  LogTestController::getInstance().reset();

  // Delete the test and state file.
    std::remove(TMP_FILE);
    std::remove(STATE_FILE);
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
