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
#include "processors/InvokeHTTP.h"
#include "processors/ListenHTTP.h"
#include "processors/LogAttribute.h"

TEST_CASE("HTTPTestsWithNoResourceClaimPOST", "[httptest1]") {
  TestController testController;
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setInfo<org::apache::nifi::minifi::processors::InvokeHTTP>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> getfileprocessor = std::make_shared<org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

  std::shared_ptr<core::Processor> logAttribute = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  std::shared_ptr<core::Processor> listenhttp = std::make_shared<org::apache::nifi::minifi::processors::ListenHTTP>("listenhttp");

  std::shared_ptr<core::Processor> invokehttp = std::make_shared<org::apache::nifi::minifi::processors::InvokeHTTP>("invokehttp");
  utils::Identifier processoruuid;
  REQUIRE(true == listenhttp->getUUID(processoruuid));

  utils::Identifier invokehttp_uuid;
  REQUIRE(true == invokehttp->getUUID(invokehttp_uuid));

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
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
  std::shared_ptr<core::ProcessContext> context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
  std::shared_ptr<core::ProcessContext> context2 = std::make_shared<core::ProcessContext>(node2, controller_services_provider, repo, repo, content_repo);
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
  std::shared_ptr<core::FlowFile> ffr = session2->get();
  REQUIRE(true == LogTestController::getInstance().contains("exiting because method is POST"));
  LogTestController::getInstance().reset();
}

class CallBack : public minifi::OutputStreamCallback {
 public:
  CallBack() {
  }
  virtual ~CallBack() {
  }
  virtual int64_t process(std::shared_ptr<minifi::io::BaseStream> stream) {
    // leaving the typo for posterity sake
    std::string st = "we're gnna write some test stuff";
    return stream->write(reinterpret_cast<uint8_t*>(const_cast<char*>(st.c_str())), st.length());
  }
};

TEST_CASE("HTTPTestsWithResourceClaimPOST", "[httptest1]") {
  TestController testController;
  LogTestController::getInstance().setInfo<org::apache::nifi::minifi::processors::InvokeHTTP>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> getfileprocessor = std::make_shared<org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

  std::shared_ptr<core::Processor> logAttribute = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  std::shared_ptr<core::Processor> listenhttp = std::make_shared<org::apache::nifi::minifi::processors::ListenHTTP>("listenhttp");

  std::shared_ptr<core::Processor> invokehttp = std::make_shared<org::apache::nifi::minifi::processors::InvokeHTTP>("invokehttp");
  utils::Identifier processoruuid;
  REQUIRE(true == listenhttp->getUUID(processoruuid));

  utils::Identifier invokehttp_uuid;
  REQUIRE(true == invokehttp->getUUID(invokehttp_uuid));

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

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
  connection2->setSourceUUID(processoruuid);

  listenhttp->addConnection(connection);
  invokehttp->addConnection(connection);
  invokehttp->addConnection(connection2);


  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(listenhttp);
  std::shared_ptr<core::ProcessorNode> node2 = std::make_shared<core::ProcessorNode>(invokehttp);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
  std::shared_ptr<core::ProcessContext> context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
  std::shared_ptr<core::ProcessContext> context2 = std::make_shared<core::ProcessContext>(node2, controller_services_provider, repo, repo, content_repo);
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

  std::map<std::string, std::string> attributes;
  attributes["testy"] = "test";
  std::shared_ptr<minifi::FlowFileRecord> flow = std::make_shared<minifi::FlowFileRecord>(repo, content_repo, attributes);
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
  std::shared_ptr<core::FlowFile> ffr = session2->get();
  REQUIRE(true == LogTestController::getInstance().contains("exiting because method is POST"));
  LogTestController::getInstance().reset();
}

TEST_CASE("HTTPTestsPostNoResourceClaim", "[httptest1]") {
  TestController testController;
  LogTestController::getInstance().setInfo<org::apache::nifi::minifi::processors::InvokeHTTP>();
  LogTestController::getInstance().setInfo<org::apache::nifi::minifi::processors::ListenHTTP>();
  LogTestController::getInstance().setInfo<core::Processor>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> processor = plan->addProcessor("ListenHTTP", "listenhttp", core::Relationship("No Retry", "description"), false);
  std::shared_ptr<core::Processor> invokehttp = plan->addProcessor("InvokeHTTP", "invokehttp", core::Relationship("success", "description"), true);

  REQUIRE(true == plan->setProperty(processor, org::apache::nifi::minifi::processors::ListenHTTP::Port.getName(), "8685"));
  REQUIRE(true == plan->setProperty(processor, org::apache::nifi::minifi::processors::ListenHTTP::BasePath.getName(), "/testytesttest"));

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
  record = plan->getCurrentFlowFile();

  for (auto provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == processor->getName());
  }
  std::shared_ptr<core::FlowFile> ffr = plan->getCurrentFlowFile();
  REQUIRE(true == LogTestController::getInstance().contains("exiting because method is POST"));
  LogTestController::getInstance().reset();
}
