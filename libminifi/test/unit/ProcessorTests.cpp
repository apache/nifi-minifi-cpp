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
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <fstream>

#include "../TestBase.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "../unit/ProvenanceTestHelper.h"
#include "core/Core.h"
#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"

TEST_CASE("Test Creation of GetFile", "[getfileCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::GetFile>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE("Test GetFileMultiple", "[getfileCreate3]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<TestRepository> repo = std::static_pointer_cast<TestRepository>(test_repo);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  uuid_t processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));

  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(test_repo, content_repo, "getfileCreate2Connection");

  connection->setRelationship(core::Relationship("success", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(processor);
  connection->setDestination(processor);

  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(processoruuid);

  processor->addConnection(connection);
  REQUIRE(dir != NULL);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
  auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);

  context->setProperty(org::apache::nifi::minifi::processors::GetFile::Directory, dir);
  // replicate 10 threads
  processor->setScheduledState(core::ScheduledState::RUNNING);

  auto factory = std::make_shared<core::ProcessSessionFactory>(context);

  processor->onSchedule(context, factory);

  int prev = 0;
  for (int i = 0; i < 10; i++) {
    auto session = std::make_shared<core::ProcessSession>(context);
    REQUIRE(processor->getName() == "getfileCreate2");

    std::shared_ptr<core::FlowFile> record;

    processor->onTrigger(context, session);

    provenance::ProvenanceReporter *reporter = session->getProvenanceReporter();
    std::set<provenance::ProvenanceEventRecord*> records = reporter->getEvents();
    record = session->get();
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
    processor->onTrigger(context, session);
    unlink(ss.str().c_str());
    reporter = session->getProvenanceReporter();

    REQUIRE(processor->getName() == "getfileCreate2");

    records = reporter->getEvents();

    for (provenance::ProvenanceEventRecord *provEventRecord : records) {
      REQUIRE(provEventRecord->getComponentType() == processor->getName());
    }
    session->commit();
    std::shared_ptr<core::FlowFile> ffr = session->get();

    REQUIRE(repo->getRepoMap().size() == (prev + 1));
    prev++;
  }
}

TEST_CASE("LogAttributeTest", "[getfileCreate3]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  testController.runSession(plan, false);
  std::set<provenance::ProvenanceEventRecord*> records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();
  testController.runSession(plan, false);

  unlink(ss.str().c_str());

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + ss.str()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + std::string(dir)));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test Find file", "[getfileCreate3]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::provenance::ProvenanceReporter>();
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> processor = plan->addProcessor("GetFile", "getfileCreate2");
  std::shared_ptr<core::Processor> processorReport = std::make_shared<org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask>(
      std::make_shared<org::apache::nifi::minifi::io::StreamFactory>(std::make_shared<org::apache::nifi::minifi::Configure>()), std::make_shared<org::apache::nifi::minifi::Configure>());
  plan->addProcessor(processorReport, "reporter", core::Relationship("success", "description"), false);
  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  plan->setProperty(processor, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  testController.runSession(plan, false);
  std::set<provenance::ProvenanceEventRecord*> records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();
  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  for (provenance::ProvenanceEventRecord *provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == processor->getName());
  }
  std::shared_ptr<core::FlowFile> ffr = plan->getCurrentFlowFile();
  REQUIRE(ffr != nullptr);
  ffr->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
  auto repo = std::static_pointer_cast<TestRepository>(plan->getProvenanceRepo());
  REQUIRE(2 == repo->getRepoMap().size());

  for (auto entry : repo->getRepoMap()) {
    provenance::ProvenanceEventRecord newRecord;
    newRecord.DeSerialize(reinterpret_cast<uint8_t*>(const_cast<char*>(entry.second.data())), entry.second.length());

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
  std::shared_ptr<org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask> taskReport = std::static_pointer_cast<
      org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask>(processorReport);
  taskReport->setBatchSize(1);
  std::vector<std::shared_ptr<core::SerializableComponent>> recordsReport;
  recordsReport.push_back(std::make_shared<provenance::ProvenanceEventRecord>());
  processorReport->incrementActiveTasks();
  processorReport->setScheduledState(core::ScheduledState::RUNNING);
  std::string jsonStr;
  std::size_t deserialized = 0;
  repo->DeSerialize(recordsReport, deserialized);
  std::function<void(const std::shared_ptr<core::ProcessContext> &, const std::shared_ptr<core::ProcessSession>&)> verifyReporter =
      [&](const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
        taskReport->getJsonReport(context, session, recordsReport, jsonStr);
        REQUIRE(recordsReport.size() == 1);
        REQUIRE(taskReport->getName() == std::string(org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask::ReportTaskName));
        REQUIRE(jsonStr.find("\"componentType\" : \"getfileCreate2\"") != std::string::npos);
      };

  testController.runSession(plan, false, verifyReporter);
}

int fileSize(const char *add) {
  std::ifstream mySource;
  mySource.open(add, std::ios_base::binary);
  mySource.seekg(0, std::ios_base::end);
  int size = mySource.tellg();
  mySource.close();
  return size;
}

