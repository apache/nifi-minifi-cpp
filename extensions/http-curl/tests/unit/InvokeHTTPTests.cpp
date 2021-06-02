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

#include <memory>
#include <utility>
#include <string>
#include "io/BaseStream.h"
#include "TestBase.h"
#include "processors/GetFile.h"
#include "core/Core.h"
#include "HTTPClient.h"
#include "InvokeHTTP.h"
#include "processors/ListenHTTP.h"
#include "core/FlowFile.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "processors/LogAttribute.h"
#include "utils/gsl.h"

TEST_CASE("HTTPTestsWithNoResourceClaimPOST", "[httptest1]") {
  TestController testController;
  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::ListenHTTP>();
  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::InvokeHTTP>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> getfileprocessor = std::make_shared<org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

  std::shared_ptr<core::Processor> logAttribute = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::shared_ptr<core::Processor> listenhttp = std::make_shared<org::apache::nifi::minifi::processors::ListenHTTP>("listenhttp");
  listenhttp->initialize();

  std::shared_ptr<core::Processor> invokehttp = std::make_shared<org::apache::nifi::minifi::processors::InvokeHTTP>("invokehttp");
  invokehttp->initialize();

  utils::Identifier processoruuid = listenhttp->getUUID();
  REQUIRE(processoruuid);

  utils::Identifier invokehttp_uuid = invokehttp->getUUID();
  REQUIRE(invokehttp_uuid);

  std::shared_ptr<minifi::Connection> gcConnection = std::make_shared<minifi::Connection>(repo, content_repo, "getfileCreate2Connection");
  gcConnection->addRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> laConnection = std::make_shared<minifi::Connection>(repo, content_repo, "logattribute");
  laConnection->addRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "getfileCreate2Connection");
  connection->addRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> connection2 = std::make_shared<minifi::Connection>(repo, content_repo, "listenhttp");

  connection2->addRelationship(core::Relationship("No Retry", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(listenhttp);

  connection2->setSourceUUID(invokehttp_uuid);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(invokehttp_uuid);

  listenhttp->addConnection(connection);
  invokehttp->addConnection(connection);
  invokehttp->addConnection(connection2);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(listenhttp);
  std::shared_ptr<core::ProcessorNode> node2 = std::make_shared<core::ProcessorNode>(invokehttp);
  std::shared_ptr<core::ProcessContext> context = std::make_shared<core::ProcessContext>(node, nullptr, repo, repo, content_repo);
  std::shared_ptr<core::ProcessContext> context2 = std::make_shared<core::ProcessContext>(node2, nullptr, repo, repo, content_repo);
  context->setProperty(org::apache::nifi::minifi::processors::ListenHTTP::Port, "8686");
  context->setProperty(org::apache::nifi::minifi::processors::ListenHTTP::BasePath, "/testytesttest");

  context2->setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::Method, "POST");
  context2->setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::URL, "http://localhost:8686/testytesttest");
  auto session = std::make_shared<core::ProcessSession>(context);
  auto session2 = std::make_shared<core::ProcessSession>(context2);

  REQUIRE(listenhttp->getName() == "listenhttp");

  std::shared_ptr<core::ProcessSessionFactory> factory = std::make_shared<core::ProcessSessionFactory>(context);

  std::shared_ptr<core::FlowFile> record;
  listenhttp->setScheduledState(core::ScheduledState::RUNNING);
  listenhttp->onSchedule(context, factory);
  listenhttp->onTrigger(context, session);

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  std::shared_ptr<core::ProcessSessionFactory> factory2 = std::make_shared<core::ProcessSessionFactory>(context2);
  invokehttp->onSchedule(context2, factory2);
  invokehttp->onTrigger(context2, session2);

  auto reporter = session->getProvenanceReporter();
  auto records = reporter->getEvents();
  record = session->get();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  listenhttp->incrementActiveTasks();
  listenhttp->setScheduledState(core::ScheduledState::RUNNING);
  listenhttp->onTrigger(context, session);

  reporter = session->getProvenanceReporter();

  records = reporter->getEvents();
  session->commit();

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  invokehttp->onTrigger(context2, session2);

  session2->commit();
  records = reporter->getEvents();

  for (auto provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == listenhttp->getName());
  }

  REQUIRE(true == LogTestController::getInstance().contains("Exiting because method is POST"));
  LogTestController::getInstance().reset();
}

class CallBack : public minifi::OutputStreamCallback {
 public:
  CallBack() {
  }
  virtual ~CallBack() {
  }
  virtual int64_t process(const std::shared_ptr<minifi::io::BaseStream>& stream) {
    // leaving the typo for posterity sake
    std::string st = "we're gnna write some test stuff";
    const auto write_ret = stream->write(reinterpret_cast<const uint8_t*>(st.c_str()), st.length());
    return minifi::io::isError(write_ret) ? -1 : gsl::narrow<int64_t>(write_ret);
  }
};

TEST_CASE("HTTPTestsWithResourceClaimPOST", "[httptest1]") {
  TestController testController;
  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::ListenHTTP>();
  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::InvokeHTTP>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> getfileprocessor = std::make_shared<org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

  std::shared_ptr<core::Processor> logAttribute = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::shared_ptr<core::Processor> listenhttp = std::make_shared<org::apache::nifi::minifi::processors::ListenHTTP>("listenhttp");
  listenhttp->initialize();

  std::shared_ptr<core::Processor> invokehttp = std::make_shared<org::apache::nifi::minifi::processors::InvokeHTTP>("invokehttp");
  invokehttp->initialize();

  utils::Identifier processoruuid = listenhttp->getUUID();
  REQUIRE(processoruuid);

  utils::Identifier invokehttp_uuid = invokehttp->getUUID();
  REQUIRE(invokehttp_uuid);

  auto configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(configuration);

  std::shared_ptr<minifi::Connection> gcConnection = std::make_shared<minifi::Connection>(repo, content_repo, "getfileCreate2Connection");
  gcConnection->addRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> laConnection = std::make_shared<minifi::Connection>(repo, content_repo, "logattribute");
  laConnection->addRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "getfileCreate2Connection");
  connection->addRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> connection2 = std::make_shared<minifi::Connection>(repo, content_repo, "listenhttp");

  connection2->addRelationship(core::Relationship("No Retry", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(listenhttp);
  connection->setSourceUUID(invokehttp_uuid);
  connection->setDestinationUUID(processoruuid);
  connection2->setSourceUUID(processoruuid);

  listenhttp->addConnection(connection);
  invokehttp->addConnection(connection);
  invokehttp->addConnection(connection2);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(listenhttp);
  std::shared_ptr<core::ProcessorNode> node2 = std::make_shared<core::ProcessorNode>(invokehttp);
  std::shared_ptr<core::ProcessContext> context = std::make_shared<core::ProcessContext>(node, nullptr, repo, repo, content_repo);
  std::shared_ptr<core::ProcessContext> context2 = std::make_shared<core::ProcessContext>(node2, nullptr, repo, repo, content_repo);
  context->setProperty(org::apache::nifi::minifi::processors::ListenHTTP::Port, "8680");
  context->setProperty(org::apache::nifi::minifi::processors::ListenHTTP::BasePath, "/testytesttest");

  context2->setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::Method, "POST");
  context2->setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::URL, "http://localhost:8680/testytesttest");
  auto session = std::make_shared<core::ProcessSession>(context);
  auto session2 = std::make_shared<core::ProcessSession>(context2);

  REQUIRE(listenhttp->getName() == "listenhttp");

  std::shared_ptr<core::ProcessSessionFactory> factory = std::make_shared<core::ProcessSessionFactory>(context);

  std::shared_ptr<core::FlowFile> record;

  CallBack callback;

  auto flow = std::make_shared<minifi::FlowFileRecord>();
  flow->setAttribute("testy", "test");
  session2->write(flow, &callback);

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  std::shared_ptr<core::ProcessSessionFactory> factory2 = std::make_shared<core::ProcessSessionFactory>(context2);
  invokehttp->onSchedule(context2, factory2);
  invokehttp->onTrigger(context2, session2);

  listenhttp->incrementActiveTasks();
  listenhttp->setScheduledState(core::ScheduledState::RUNNING);
  listenhttp->onSchedule(context, factory);
  listenhttp->onTrigger(context, session);

  auto reporter = session->getProvenanceReporter();
  auto records = reporter->getEvents();
  record = session->get();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  listenhttp->incrementActiveTasks();
  listenhttp->setScheduledState(core::ScheduledState::RUNNING);
  listenhttp->onTrigger(context, session);

  reporter = session->getProvenanceReporter();

  records = reporter->getEvents();
  session->commit();

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  invokehttp->onTrigger(context2, session2);

  session2->commit();
  records = reporter->getEvents();

  for (auto provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == listenhttp->getName());
  }

  REQUIRE(true == LogTestController::getInstance().contains("Exiting because method is POST"));
  LogTestController::getInstance().reset();
}

TEST_CASE("HTTPTestsPostNoResourceClaim", "[httptest1]") {
  TestController testController;
  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::ListenHTTP>();
  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::InvokeHTTP>();
  LogTestController::getInstance().setInfo<core::Processor>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> listenhttp = plan->addProcessor("ListenHTTP", "listenhttp", core::Relationship("No Retry", "description"), false);
  listenhttp->initialize();
  std::shared_ptr<core::Processor> invokehttp = plan->addProcessor("InvokeHTTP", "invokehttp", core::Relationship("success", "description"), true);
  invokehttp->initialize();

  REQUIRE(true == plan->setProperty(listenhttp, org::apache::nifi::minifi::processors::ListenHTTP::Port.getName(), "8685"));
  REQUIRE(true == plan->setProperty(listenhttp, org::apache::nifi::minifi::processors::ListenHTTP::BasePath.getName(), "/testytesttest"));

  REQUIRE(true == plan->setProperty(invokehttp, org::apache::nifi::minifi::processors::InvokeHTTP::Method.getName(), "POST"));
  REQUIRE(true == plan->setProperty(invokehttp, org::apache::nifi::minifi::processors::InvokeHTTP::URL.getName(), "http://localhost:8685/testytesttest"));
  plan->reset();
  testController.runSession(plan, true);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  plan->reset();
  testController.runSession(plan, true);

  records = plan->getProvenanceRecords();

  for (auto provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == listenhttp->getName());
  }

  REQUIRE(true == LogTestController::getInstance().contains("Exiting because method is POST"));
  LogTestController::getInstance().reset();
}
