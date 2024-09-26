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
#define EXTENSION_LIST "*minifi-*,!*splunk*,!*elastic*,!*grafana-loki*"  // NOLINT(cppcoreguidelines-macro-usage)

#include <cstdio>
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <GenerateFlowFile.h>
#ifdef WIN32
#include <fileapi.h>
#include <system_error>
#endif /* WIN32 */

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "LogAttribute.h"
#include "GetFile.h"
#include "unit/ProvenanceTestHelper.h"
#include "utils/file/FileUtils.h"
#include "core/Core.h"
#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"
#include "core/Resource.h"
#include "utils/gsl.h"
#include "utils/PropertyErrors.h"
#include "unit/TestUtils.h"
#include "io/BufferStream.h"
#include "fmt/format.h"

TEST_CASE("Test Creation of GetFile", "[getfileCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::GetFile>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

// TODO(adebreceni)
// what is this test? multiple onTriggers with the same session
// session->get() then no commit, same repo for flowFileRepo and provenance repo
TEST_CASE("Test GetFileMultiple", "[getfileCreate3]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  auto config = std::make_shared<minifi::ConfigureImpl>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(config);
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");
  processor->initialize();
  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<TestRepository> repo = std::dynamic_pointer_cast<TestRepository>(test_repo);

  auto dir = testController.createTempDirectory();
  utils::Identifier processoruuid = processor->getUUID();
  REQUIRE(processoruuid);

  auto connection = std::make_unique<minifi::ConnectionImpl>(test_repo, content_repo, "getfileCreate2Connection");

  connection->addRelationship(core::Relationship("success", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(processor.get());
  connection->setDestination(processor.get());

  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(processoruuid);

  processor->addConnection(connection.get());
  REQUIRE(!dir.empty());

  auto node = std::make_shared<core::ProcessorNodeImpl>(processor.get());
  auto context = std::make_shared<core::ProcessContextImpl>(node, nullptr, repo, repo, content_repo);

  context->setProperty(org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  // replicate 10 threads
  processor->setScheduledState(core::ScheduledState::RUNNING);

  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context);

  processor->onSchedule(*context, *factory);

  for (int i = 1; i < 10; i++) {
    auto session = std::make_shared<core::ProcessSessionImpl>(context);
    REQUIRE(processor->getName() == "getfileCreate2");

    std::shared_ptr<core::FlowFile> record;

    processor->onTrigger(*context, *session);

    auto reporter = session->getProvenanceReporter();
    auto records = reporter->getEvents();
    record = session->get();
    REQUIRE(record == nullptr);
    REQUIRE(records.empty());

    std::fstream file;
    auto path = dir / "tstFile.ext";
    file.open(path, std::ios::out);
    file << "tempFile";
    file.close();

    processor->incrementActiveTasks();
    processor->setScheduledState(core::ScheduledState::RUNNING);
    processor->onTrigger(*context, *session);
    std::filesystem::remove(path);
    reporter = session->getProvenanceReporter();

    REQUIRE(processor->getName() == "getfileCreate2");

    records = reporter->getEvents();

    for (const auto& provEventRecord : records) {
      REQUIRE(provEventRecord->getComponentType() == processor->getName());
    }
    session->commit();
    std::shared_ptr<core::FlowFile> ffr = session->get();
    REQUIRE(ffr);  // GetFile successfully read the contents and created a flowFile

    // one CREATE, one MODIFY, and one FF contents, as we use the same
    // underlying repo for both provenance and flowFileRepo
    REQUIRE(repo->getRepoMap().size() == gsl::narrow<size_t>(3 * i));
  }
}

TEST_CASE("Test GetFile Ignore", "[getfileCreate3]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");
  processor->initialize();

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<TestRepository> repo = std::dynamic_pointer_cast<TestRepository>(test_repo);

  const auto dir = testController.createTempDirectory();

  utils::Identifier processoruuid = processor->getUUID();
  REQUIRE(processoruuid);

  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::ConnectionImpl>(test_repo, content_repo, "getfileCreate2Connection");

  connection->addRelationship(core::Relationship("success", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(processor.get());
  connection->setDestination(processor.get());

  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(processoruuid);

  processor->addConnection(connection.get());
  REQUIRE(!dir.empty());

  auto node = std::make_shared<core::ProcessorNodeImpl>(processor.get());
  auto context = std::make_shared<core::ProcessContextImpl>(node, nullptr, repo, repo, content_repo);

  context->setProperty(org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  // replicate 10 threads
  processor->setScheduledState(core::ScheduledState::RUNNING);

  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context);

  processor->onSchedule(*context, *factory);

  auto session = std::make_shared<core::ProcessSessionImpl>(context);
  REQUIRE(processor->getName() == "getfileCreate2");

  std::shared_ptr<core::FlowFile> record;

  processor->onTrigger(*context, *session);

  auto reporter = session->getProvenanceReporter();
  auto records = reporter->getEvents();
  record = session->get();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  const std::filesystem::path hidden_file_name = dir / ".filewithoutanext";
  {
    std::ofstream file{ hidden_file_name };
    file << "tempFile";
  }

#ifdef WIN32
  {
    // hide file on windows, because a . prefix in the filename doesn't imply a hidden file
    const auto hide_file_error = minifi::test::utils::hide_file(hidden_file_name.c_str());
    REQUIRE(!hide_file_error);
  }
#endif /* WIN32 */

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onTrigger(*context, *session);
  std::filesystem::remove(hidden_file_name);
  reporter = session->getProvenanceReporter();

  REQUIRE(processor->getName() == "getfileCreate2");

  records = reporter->getEvents();

  for (const auto& provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == processor->getName());
  }
  session->commit();
  std::shared_ptr<core::FlowFile> ffr = session->get();

  REQUIRE(repo->getRepoMap().empty());
}

TEST_CASE("TestConnectionFull", "[ConnectionFull]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::GenerateFlowFile>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(std::make_shared<minifi::ConfigureImpl>());
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::GenerateFlowFile>("GFF");
  processor->initialize();
  processor->setProperty(minifi::processors::GenerateFlowFile::BatchSize, "10");
  processor->setProperty(minifi::processors::GenerateFlowFile::FileSize, "0");

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<TestRepository> repo = std::dynamic_pointer_cast<TestRepository>(test_repo);

  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::ConnectionImpl>(test_repo, content_repo, "GFF2Connection");
  connection->setBackpressureThresholdCount(5);
  connection->addRelationship(core::Relationship("success", "description"));

  utils::Identifier processoruuid = processor->getUUID();

  // link the connections so that we can test results at the end for this
  connection->setSource(processor.get());
  connection->setDestination(processor.get());

  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(processoruuid);

  processor->addConnection(connection.get());
  processor->setScheduledState(core::ScheduledState::RUNNING);

  auto node = std::make_shared<core::ProcessorNodeImpl>(processor.get());
  auto context = std::make_shared<core::ProcessContextImpl>(node, nullptr, repo, repo, content_repo);

  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context);

  processor->onSchedule(*context, *factory);

  auto session = std::make_shared<core::ProcessSessionImpl>(context);

  CHECK_FALSE(session->outgoingConnectionsFull("success"));
  CHECK_FALSE(connection->backpressureThresholdReached());

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onTrigger(*context, *session);

  session->commit();

  CHECK(connection->backpressureThresholdReached());
  CHECK(session->outgoingConnectionsFull("success"));
}

TEST_CASE("LogAttributeTest", "[getfileCreate3]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  auto dir = testController.createTempDirectory();

  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  testController.runSession(plan, false);
  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  auto path = dir / "tstFile.ext";
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();
  testController.runSession(plan, false);

  std::filesystem::remove(path);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();

  CHECK(true == LogTestController::getInstance().contains("key:absolute.path value:" + (dir / "").string()));
  CHECK(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  CHECK(true == LogTestController::getInstance().contains("key:path value:" + (std::filesystem::path(".") / "").string()));
  LogTestController::getInstance().reset();
}

TEST_CASE("LogAttributeTestInvalid", "[TestLogAttribute]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");

  auto loggattr = plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  auto dir = testController.createTempDirectory();

  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::BatchSize, "1");
  REQUIRE_THROWS_AS(plan->setProperty(loggattr, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, "-1"), utils::internal::ParseException);
  LogTestController::getInstance().reset();
}

void testMultiplesLogAttribute(int fileCount, int flowsToLog, std::string verifyStringFlowsLogged = "") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");

  auto loggattr = plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  auto dir = testController.createTempDirectory();

  auto flowsToLogStr = std::to_string(flowsToLog);
  if (verifyStringFlowsLogged.empty())
    verifyStringFlowsLogged = std::to_string(flowsToLog);
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::BatchSize, std::to_string(fileCount));
  plan->setProperty(loggattr, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, flowsToLogStr);
  testController.runSession(plan, false);
  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::vector<std::filesystem::path> files;

  for (int i = 0; i < fileCount; i++) {
    std::fstream file;
    auto path = dir / ("tstFile" + std::to_string(i) + ".ext");
    file.open(path, std::ios::out);
    file << "tempFile";
    file.close();

    files.push_back(path);
  }

  plan->reset();
  testController.runSession(plan, false);

  for (const auto& created_file : files) {
    std::filesystem::remove(created_file);
  }

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();

  CHECK(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  CHECK(true == LogTestController::getInstance().contains("key:path value:" + (std::filesystem::path(".") / "").string()));
  CHECK(true == LogTestController::getInstance().contains("Logged " + verifyStringFlowsLogged + " flow files"));
  LogTestController::getInstance().reset();
}

TEST_CASE("LogAttributeTestSingle", "[TestLogAttribute]") {
  testMultiplesLogAttribute(1, 3, "1");
}

TEST_CASE("LogAttributeTestMultiple", "[TestLogAttribute]") {
  testMultiplesLogAttribute(5, 3);
}

TEST_CASE("LogAttributeTestAll", "[TestLogAttribute]") {
  testMultiplesLogAttribute(5, 0, "5");
}

TEST_CASE("Test Find file", "[getfileCreate3]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::provenance::ProvenanceReporter>();
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> processor = plan->addProcessor("GetFile", "getfileCreate2");
  std::shared_ptr<core::Processor> processorReport = std::make_shared<org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask>(
      std::make_shared<org::apache::nifi::minifi::ConfigureImpl>());
  plan->addProcessor(processorReport, "reporter", core::Relationship("success", "description"), false);

  auto dir = testController.createTempDirectory();
  plan->setProperty(processor, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  testController.runSession(plan, false);
  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  auto path = dir / "tstFile.ext";
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();
  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  for (const auto& provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == processor->getName());
  }
  std::shared_ptr<core::FlowFile> ffr = plan->getCurrentFlowFile();
  REQUIRE(ffr != nullptr);
  ffr->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
  auto repo = std::dynamic_pointer_cast<TestRepository>(plan->getProvenanceRepo());
  REQUIRE(2 == repo->getRepoMap().size());

  for (auto entry : repo->getRepoMap()) {
    minifi::provenance::ProvenanceEventRecordImpl newRecord;
    minifi::io::BufferStream stream(gsl::make_span(entry.second).as_span<const std::byte>());
    newRecord.deserialize(stream);

    bool found = false;
    for (const auto& provRec : records) {
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
  std::shared_ptr<org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask> taskReport = std::dynamic_pointer_cast<
      org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask>(processorReport);
  taskReport->setBatchSize(1);
  std::vector<std::shared_ptr<core::SerializableComponent>> recordsReport;
  recordsReport.push_back(std::make_shared<minifi::provenance::ProvenanceEventRecordImpl>());
  processorReport->incrementActiveTasks();
  processorReport->setScheduledState(core::ScheduledState::RUNNING);
  std::string jsonStr;
  std::size_t deserialized = 0;
  repo->getElements(recordsReport, deserialized);
  std::function<void(const std::shared_ptr<core::ProcessContext> &, const std::shared_ptr<core::ProcessSession>&)> verifyReporter =
      [&](const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
        taskReport->getJsonReport(*context, *session, recordsReport, jsonStr);
        REQUIRE(recordsReport.size() == 1);
        REQUIRE(taskReport->getName() == std::string(org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask::ReportTaskName));
        REQUIRE(jsonStr.find("\"componentType\": \"getfileCreate2\"") != std::string::npos);
      };

  testController.runSession(plan, false, verifyReporter);
}

class TestProcessorNoContent : public minifi::core::ProcessorImpl {
 public:
  explicit TestProcessorNoContent(std::string_view name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
  }

  static constexpr const char* Description = "test resource";
  static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  static constexpr auto Relationships = std::array{Success};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) override {}
  void onTrigger(core::ProcessContext&, core::ProcessSession& session) override {
    auto ff = session.create();
    ff->addAttribute("Attribute", "AttributeValue");
    session.transfer(ff, Success);
  }
};

REGISTER_RESOURCE(TestProcessorNoContent, Processor);

TEST_CASE("TestEmptyContent", "[emptyContent]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
  LogTestController::getInstance().setDebug<minifi::core::ProcessSession>();
  LogTestController::getInstance().setDebug<TestPlan>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> getfile = plan->addProcessor("TestProcessorNoContent", "TestProcessorNoContent");

  plan->runNextProcessor();

  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds{0}, "did not create a ResourceClaim"));

  LogTestController::getInstance().reset();
}

/**
 * Tests the RPG bypass feature
 * @param host to configure
 * @param port port string to configure
 * @param portVal port value to search in the corresponding log message
 * @param hasException dictates if a failure should occur
 */
void testRPGBypass(const std::string &host, const std::string &port, bool has_error = true) {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::RemoteProcessorGroupPort>();
  LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
  LogTestController::getInstance().setTrace<TestPlan>();

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<TestRepository> repo = std::dynamic_pointer_cast<TestRepository>(test_repo);

  auto rpg = std::make_shared<minifi::RemoteProcessorGroupPort>("rpg", "http://localhost:8989/nifi", configuration);
  rpg->initialize();
  REQUIRE(rpg->setProperty(minifi::RemoteProcessorGroupPort::hostName, host));
  rpg->setProperty(minifi::RemoteProcessorGroupPort::port, port);
  auto node = std::make_shared<core::ProcessorNodeImpl>(rpg.get());
  auto context = std::make_shared<core::ProcessContextImpl>(node, nullptr, repo, repo, content_repo);
  auto psf = std::make_shared<core::ProcessSessionFactoryImpl>(context);
  if (has_error) {
    rpg->onSchedule(*context, *psf);
    auto expected_error = "No peers selected during scheduling";
    REQUIRE(LogTestController::getInstance().contains(expected_error));
  }
  LogTestController::getInstance().reset();
}

TEST_CASE("TestRPGNoSettings", "[TestRPG1]") {
  testRPGBypass("", "");
}

TEST_CASE("TestRPGWithHost", "[TestRPG2]") {
  testRPGBypass("hostname", "");
}

TEST_CASE("TestRPGWithHostInvalidPort", "[TestRPG3]") {
  testRPGBypass("hostname", "hostname");
}

TEST_CASE("TestRPGWithoutHostValidPort", "[TestRPG4]") {
  testRPGBypass("", "8080");
}

TEST_CASE("TestRPGWithoutHostInvalidPort", "[TestRPG5]") {
  testRPGBypass("", "hostname");
}

TEST_CASE("TestRPGValid", "[TestRPG6]") {
  testRPGBypass("", "8080", false);
}

namespace {

class ProcessorWithIncomingConnectionTest {
 public:
  ProcessorWithIncomingConnectionTest();
  ProcessorWithIncomingConnectionTest(ProcessorWithIncomingConnectionTest&&) = delete;
  ProcessorWithIncomingConnectionTest(const ProcessorWithIncomingConnectionTest&) = delete;
  ProcessorWithIncomingConnectionTest& operator=(ProcessorWithIncomingConnectionTest&&) = delete;
  ProcessorWithIncomingConnectionTest& operator=(const ProcessorWithIncomingConnectionTest&) = delete;
  ~ProcessorWithIncomingConnectionTest();

 protected:
  std::shared_ptr<core::Processor> processor_;
  std::shared_ptr<minifi::Connection> incoming_connection_;
  std::shared_ptr<core::ProcessSession> session_;
};

ProcessorWithIncomingConnectionTest::ProcessorWithIncomingConnectionTest() {
  LogTestController::getInstance().setDebug<core::Processor>();

  const auto repo = std::make_shared<TestRepository>();
  const auto content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(std::make_shared<minifi::ConfigureImpl>());

  processor_ = std::make_shared<minifi::processors::LogAttribute>("test_processor");
  incoming_connection_ = std::make_shared<minifi::ConnectionImpl>(repo, content_repo, "incoming_connection");
  incoming_connection_->addRelationship(core::Relationship{"success", ""});
  incoming_connection_->setDestinationUUID(processor_->getUUID());
  processor_->addConnection(incoming_connection_.get());
  processor_->initialize();

  const auto processor_node = std::make_shared<core::ProcessorNodeImpl>(processor_.get());
  const auto context = std::make_shared<core::ProcessContextImpl>(processor_node, nullptr, repo, repo, content_repo);
  const auto session_factory = std::make_shared<core::ProcessSessionFactoryImpl>(context);
  session_ = session_factory->createSession();
}

ProcessorWithIncomingConnectionTest::~ProcessorWithIncomingConnectionTest() {
  LogTestController::getInstance().reset();
}

}  // namespace

TEST_CASE_METHOD(ProcessorWithIncomingConnectionTest, "A Processor detects correctly if it has incoming flow files it can process", "[isWorkAvailable]") {
  SECTION("Initially, the queue is empty, so there is no work available") {
    REQUIRE_FALSE(processor_->isWorkAvailable());
  }

  SECTION("When a non-penalized flow file is queued, there is work available") {
    const auto flow_file = session_->create();
    incoming_connection_->put(flow_file);

    REQUIRE(processor_->isWorkAvailable());
  }

  SECTION("When a penalized flow file is queued, there is no work available (until the penalty expires)") {
    const auto flow_file = session_->create();
    session_->penalize(flow_file);
    incoming_connection_->put(flow_file);

    REQUIRE_FALSE(processor_->isWorkAvailable());
  }

  SECTION("If there is both a penalized and a non-penalized flow file queued, there is work available") {
    const auto normal_flow_file = session_->create();
    incoming_connection_->put(normal_flow_file);

    const auto penalized_flow_file = session_->create();
    session_->penalize(penalized_flow_file);
    incoming_connection_->put(penalized_flow_file);

    REQUIRE(processor_->isWorkAvailable());
  }

  SECTION("When a penalized flow file is queued, there is work available after the penalty expires") {
    processor_->setPenalizationPeriod(std::chrono::milliseconds{10});

    const auto flow_file = session_->create();
    session_->penalize(flow_file);
    incoming_connection_->put(flow_file);

    REQUIRE_FALSE(processor_->isWorkAvailable());
    const auto penalty_has_expired = [flow_file] { return !flow_file->isPenalized(); };
    REQUIRE(minifi::test::utils::verifyEventHappenedInPollTime(std::chrono::seconds{1}, penalty_has_expired, std::chrono::milliseconds{10}));
    REQUIRE(processor_->isWorkAvailable());
  }
}

TEST_CASE_METHOD(ProcessorWithIncomingConnectionTest, "A failed and re-penalized flow file does not block the incoming queue of the Processor", "[penalize]") {
  processor_->setPenalizationPeriod(std::chrono::milliseconds{100});
  const auto penalized_flow_file = session_->create();
  session_->penalize(penalized_flow_file);
  incoming_connection_->put(penalized_flow_file);
  const auto penalty_has_expired = [penalized_flow_file] { return !penalized_flow_file->isPenalized(); };
  REQUIRE(minifi::test::utils::verifyEventHappenedInPollTime(std::chrono::seconds{1}, penalty_has_expired, std::chrono::milliseconds{10}));

  const auto flow_file_1 = session_->create();
  incoming_connection_->put(flow_file_1);
  const auto flow_file_2 = session_->create();
  incoming_connection_->put(flow_file_2);
  const auto flow_file_3 = session_->create();
  incoming_connection_->put(flow_file_3);

  REQUIRE(incoming_connection_->isWorkAvailable());
  std::set<std::shared_ptr<core::FlowFile>> expired_flow_files;
  const auto next_flow_file_1 = incoming_connection_->poll(expired_flow_files);
  REQUIRE(next_flow_file_1 == penalized_flow_file);

  session_->penalize(penalized_flow_file);
  incoming_connection_->put(penalized_flow_file);
  std::this_thread::sleep_for(std::chrono::milliseconds{110});

  REQUIRE(incoming_connection_->isWorkAvailable());
  const auto next_flow_file_2 = incoming_connection_->poll(expired_flow_files);
  REQUIRE(next_flow_file_2 != penalized_flow_file);
  REQUIRE(next_flow_file_2 == flow_file_1);

  REQUIRE(incoming_connection_->isWorkAvailable());
  const auto next_flow_file_3 = incoming_connection_->poll(expired_flow_files);
  REQUIRE(next_flow_file_3 != penalized_flow_file);
  REQUIRE(next_flow_file_3 == flow_file_2);

  REQUIRE(incoming_connection_->isWorkAvailable());
  const auto next_flow_file_4 = incoming_connection_->poll(expired_flow_files);
  REQUIRE(next_flow_file_4 != penalized_flow_file);
  REQUIRE(next_flow_file_4 == flow_file_3);

  REQUIRE(minifi::test::utils::verifyEventHappenedInPollTime(std::chrono::seconds{1}, penalty_has_expired, std::chrono::milliseconds{10}));
  REQUIRE(incoming_connection_->isWorkAvailable());
  const auto next_flow_file_5 = incoming_connection_->poll(expired_flow_files);
  REQUIRE(next_flow_file_5 == penalized_flow_file);
}

TEST_CASE("InputRequirementTestOk", "[InputRequirement]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::GenerateFlowFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  plan->addProcessor("GenerateFlowFile", "generateFlowFile");
  plan->addProcessor("LogAttribute", "logAttribute", core::Relationship("success", "description"), true);

  REQUIRE_NOTHROW(plan->validateAnnotations());
  testController.runSession(plan);
}

TEST_CASE("InputRequirementTestForbidden", "[InputRequirement]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::GenerateFlowFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  plan->addProcessor("GenerateFlowFile", "generateFlowFile");
  auto gen2_proc = plan->addProcessor("GenerateFlowFile", "generateFlowFile2", core::Relationship("success", "description"), true);

  REQUIRE_THROWS_WITH(plan->validateAnnotations(), Catch::Matchers::EndsWith(
    fmt::format("INPUT_FORBIDDEN was specified for the processor 'generateFlowFile2' (uuid: '{}'), but there are incoming connections", std::string(gen2_proc->getUUIDStr()))));
  testController.runSession(plan);
}

TEST_CASE("InputRequirementTestRequired", "[InputRequirement]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto log_proc = plan->addProcessor("LogAttribute", "logAttribute");
  plan->addProcessor("LogAttribute", "logAttribute2", core::Relationship("success", "description"), true);

  REQUIRE_THROWS_WITH(plan->validateAnnotations(), Catch::Matchers::EndsWith(
    fmt::format("INPUT_REQUIRED was specified for the processor 'logAttribute' (uuid: '{}'), but no incoming connections were found", std::string(log_proc->getUUIDStr()))));
  testController.runSession(plan);
}

TEST_CASE("isSingleThreaded - one thread for a multithreaded processor", "[isSingleThreaded]") {
  TestController testController;

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto processor = plan->addProcessor("GenerateFlowFile", "myProc");
  // default max concurrent tasks value is 1 for every processor

  REQUIRE_NOTHROW(plan->validateAnnotations());
  REQUIRE(processor->getMaxConcurrentTasks() == 1);
}

TEST_CASE("isSingleThreaded - two threads for a multithreaded processor", "[isSingleThreaded]") {
  TestController testController;

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto processor = plan->addProcessor("GenerateFlowFile", "myProc");
  processor->setMaxConcurrentTasks(2);

  REQUIRE_NOTHROW(plan->validateAnnotations());
  REQUIRE(processor->getMaxConcurrentTasks() == 2);
}

TEST_CASE("isSingleThreaded - one thread for a single threaded processor", "[isSingleThreaded]") {
  TestController testController;

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto processor = plan->addProcessor("TailFile", "myProc");
  // default max concurrent tasks value is 1 for every processor

  REQUIRE_NOTHROW(plan->validateAnnotations());
  REQUIRE(processor->getMaxConcurrentTasks() == 1);
}

TEST_CASE("isSingleThreaded - two threads for a single threaded processor", "[isSingleThreaded]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::core::Processor>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto processor = plan->addProcessor("TailFile", "myProc");
  processor->setMaxConcurrentTasks(2);

  REQUIRE_NOTHROW(plan->validateAnnotations());
  REQUIRE(processor->getMaxConcurrentTasks() == 1);
  REQUIRE(LogTestController::getInstance().contains("[warning] Processor myProc can not be run in parallel, its "
                                                    "\"max concurrent tasks\" value is too high. It was set to 1 from 2."));
}

TEST_CASE("Test getProcessorType", "[getProcessorType]") {
  TestController testController;

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto processor = plan->addProcessor("GenerateFlowFile", "myProc");
  REQUIRE(processor->getProcessorType() == "GenerateFlowFile");
}

TEST_CASE("IsYield and getYieldTime is consistent") {
  auto processor = TestProcessorNoContent("test_processor");
  processor.yield(1ms);
  REQUIRE(processor.isYield() == (processor.getYieldTime() != 0ms));
}
