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
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <uuid/uuid.h>
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <fstream>
#include "../unit/ProvenanceTestHelper.h"
#include "../TestBase.h"
#include "processors/ListenHTTP.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "core/Core.h"
#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"

TEST_CASE("Test Creation of GetFile", "[getfileCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = std::make_shared<
      org::apache::nifi::minifi::processors::GetFile>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE("Test Find file", "[getfileCreate2]") {
  TestController testController;

  std::shared_ptr<core::Processor> processor = std::make_shared<
      org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

  std::shared_ptr<core::Processor> processorReport =
      std::make_shared<
          org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask>(
          std::make_shared<org::apache::nifi::minifi::io::StreamFactory>(
              std::make_shared<org::apache::nifi::minifi::Configure>()));

  std::shared_ptr<core::Repository> test_repo =
      std::make_shared<TestRepository>();

  std::shared_ptr<TestRepository> repo =
      std::static_pointer_cast<TestRepository>(test_repo);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  uuid_t processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));

  std::shared_ptr<minifi::Connection> connection = std::make_shared<
      minifi::Connection>(test_repo, "getfileCreate2Connection");
  connection->setRelationship(core::Relationship("success", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(processor);
  connection->setDestination(processor);

  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(processoruuid);

  processor->addConnection(connection);
  REQUIRE(dir != NULL);

  core::ProcessorNode node(processor);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider =
      nullptr;
  core::ProcessContext context(node, controller_services_provider, test_repo);
  core::ProcessSessionFactory factory(&context);
  context.setProperty(org::apache::nifi::minifi::processors::GetFile::Directory,
                      dir);
  core::ProcessSession session(&context);

  processor->onSchedule(&context, &factory);
  REQUIRE(processor->getName() == "getfileCreate2");

  std::shared_ptr<core::FlowFile> record;
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onTrigger(&context, &session);

  provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
  std::set<provenance::ProvenanceEventRecord*> records = reporter->getEvents();
  record = session.get();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onTrigger(&context, &session);
  unlink(ss.str().c_str());
  reporter = session.getProvenanceReporter();

  REQUIRE(processor->getName() == "getfileCreate2");

  records = reporter->getEvents();

  for (provenance::ProvenanceEventRecord *provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == processor->getName());
  }
  session.commit();
  std::shared_ptr<core::FlowFile> ffr = session.get();

  ffr->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
  REQUIRE(2 == repo->getRepoMap().size());

  for (auto entry : repo->getRepoMap()) {
    provenance::ProvenanceEventRecord newRecord;
    newRecord.DeSerialize(
        reinterpret_cast<uint8_t*>(const_cast<char*>(entry.second.data())),
        entry.second.length());

    bool found = false;
    for (auto provRec : records) {
      if (provRec->getEventId() == newRecord.getEventId()) {
        REQUIRE(provRec->getEventId() == newRecord.getEventId());
        REQUIRE(provRec->getComponentId() == newRecord.getComponentId());
        REQUIRE(provRec->getComponentType() == newRecord.getComponentType());
        REQUIRE(provRec->getDetails() == newRecord.getDetails());
        REQUIRE(provRec->getEventDuration() == newRecord.getEventDuration());
        found = true;
        break;
      }
    }
    if (!found) {
      throw std::runtime_error("Did not find record");
    }
  }

  core::ProcessorNode nodeReport(processorReport);
  core::ProcessContext contextReport(nodeReport, controller_services_provider,
                                     test_repo);
  core::ProcessSessionFactory factoryReport(&contextReport);
  core::ProcessSession sessionReport(&contextReport);
  processorReport->onSchedule(&contextReport, &factoryReport);
  std::shared_ptr<
      org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask> taskReport =
      std::static_pointer_cast<
          org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask>(
          processorReport);
  taskReport->setBatchSize(1);
  std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> recordsReport;
  processorReport->incrementActiveTasks();
  processorReport->setScheduledState(core::ScheduledState::RUNNING);
  std::string jsonStr;
  repo->getProvenanceRecord(recordsReport, 1);
  taskReport->getJsonReport(&contextReport, &sessionReport, recordsReport,
                            jsonStr);
  REQUIRE(recordsReport.size() == 1);
  REQUIRE(
      taskReport->getName()
          == std::string(
              org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask::ReportTaskName));
  REQUIRE(
      jsonStr.find("\"componentType\" : \"getfileCreate2\"")
          != std::string::npos);
}

TEST_CASE("Test GetFileLikeIt'sThreaded", "[getfileCreate3]") {
  TestController testController;

  std::shared_ptr<core::Processor> processor = std::make_shared<
      org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

  std::shared_ptr<core::Repository> test_repo =
      std::make_shared<TestRepository>();

  std::shared_ptr<TestRepository> repo =
      std::static_pointer_cast<TestRepository>(test_repo);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  uuid_t processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));

  std::shared_ptr<minifi::Connection> connection = std::make_shared<
      minifi::Connection>(test_repo, "getfileCreate2Connection");
  connection->setRelationship(core::Relationship("success", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(processor);
  connection->setDestination(processor);

  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(processoruuid);

  processor->addConnection(connection);
  REQUIRE(dir != NULL);

  core::ProcessorNode node(processor);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider =
      nullptr;
  core::ProcessContext context(node, controller_services_provider, test_repo);
  core::ProcessSessionFactory factory(&context);
  context.setProperty(org::apache::nifi::minifi::processors::GetFile::Directory,
                      dir);
  // replicate 10 threads
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onSchedule(&context, &factory);

  int prev = 0;
  for (int i = 0; i < 10; i++) {
    core::ProcessSession session(&context);
    REQUIRE(processor->getName() == "getfileCreate2");

    std::shared_ptr<core::FlowFile> record;

    processor->onTrigger(&context, &session);

    provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
    std::set<provenance::ProvenanceEventRecord*> records =
        reporter->getEvents();
    record = session.get();
    REQUIRE(record == nullptr);
    REQUIRE(records.size() == 0);

    std::fstream file;
    std::stringstream ss;
    ss << dir << "/" << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();

    processor->incrementActiveTasks();
    processor->setScheduledState(core::ScheduledState::RUNNING);
    processor->onTrigger(&context, &session);
    unlink(ss.str().c_str());
    reporter = session.getProvenanceReporter();

    REQUIRE(processor->getName() == "getfileCreate2");

    records = reporter->getEvents();

    for (provenance::ProvenanceEventRecord *provEventRecord : records) {
      REQUIRE(provEventRecord->getComponentType() == processor->getName());
    }
    session.commit();
    std::shared_ptr<core::FlowFile> ffr = session.get();

    REQUIRE((repo->getRepoMap().size() % 2) == 0);
    REQUIRE(repo->getRepoMap().size() == (prev + 2));
    prev += 2;
  }
}

TEST_CASE("LogAttributeTest", "[getfileCreate3]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<core::Repository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<
      org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

  std::shared_ptr<core::Processor> logAttribute = std::make_shared<
      org::apache::nifi::minifi::processors::LogAttribute>("logattribute");

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  uuid_t processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));

  uuid_t logattribute_uuid;
  REQUIRE(true == logAttribute->getUUID(logattribute_uuid));

  std::shared_ptr<minifi::Connection> connection = std::make_shared<
      minifi::Connection>(repo, "getfileCreate2Connection");
  connection->setRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> connection2 = std::make_shared<
      minifi::Connection>(repo, "logattribute");
  connection2->setRelationship(core::Relationship("success", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(processor);

  // link the connections so that we can test results at the end for this
  connection->setDestination(logAttribute);

  connection2->setSource(logAttribute);

  connection2->setSourceUUID(logattribute_uuid);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(logattribute_uuid);

  processor->addConnection(connection);
  logAttribute->addConnection(connection);
  logAttribute->addConnection(connection2);
  REQUIRE(dir != NULL);

  core::ProcessorNode node(processor);
  core::ProcessorNode node2(logAttribute);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider =
      nullptr;
  core::ProcessContext context(node, controller_services_provider, repo);
  core::ProcessContext context2(node2, controller_services_provider, repo);
  context.setProperty(org::apache::nifi::minifi::processors::GetFile::Directory,
                      dir);
  core::ProcessSession session(&context);
  core::ProcessSession session2(&context2);

  REQUIRE(processor->getName() == "getfileCreate2");

  std::shared_ptr<core::FlowFile> record;
  processor->setScheduledState(core::ScheduledState::RUNNING);

  core::ProcessSessionFactory factory(&context);
  processor->onSchedule(&context, &factory);
  processor->onTrigger(&context, &session);

  logAttribute->incrementActiveTasks();
  logAttribute->setScheduledState(core::ScheduledState::RUNNING);
  core::ProcessSessionFactory factory2(&context2);
  logAttribute->onSchedule(&context2, &factory2);
  logAttribute->onTrigger(&context2, &session2);

  provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
  std::set<provenance::ProvenanceEventRecord*> records = reporter->getEvents();
  record = session.get();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onTrigger(&context, &session);
  unlink(ss.str().c_str());
  reporter = session.getProvenanceReporter();

  records = reporter->getEvents();
  session.commit();

  logAttribute->incrementActiveTasks();
  logAttribute->setScheduledState(core::ScheduledState::RUNNING);
  logAttribute->onTrigger(&context2, &session2);

  records = reporter->getEvents();

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + ss.str()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + std::string(dir)));
  LogTestController::getInstance().reset();
}

int fileSize(const char *add) {
  std::ifstream mySource;
  mySource.open(add, std::ios_base::binary);
  mySource.seekg(0, std::ios_base::end);
  int size = mySource.tellg();
  mySource.close();
  return size;
}

