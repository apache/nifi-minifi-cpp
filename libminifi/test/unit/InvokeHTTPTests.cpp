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
#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <set>
#include "FlowController.h"
#include "../TestBase.h"
#include "processors/GetFile.h"
#include "core/Core.h"
#include "../../include/core/FlowFile.h"
#include "../unit/ProvenanceTestHelper.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"

TEST_CASE("HTTPTestsPostNoResourceClaim", "[httptest1]") {
  TestController testController;
  LogTestController::getInstance().setInfo<org::apache::nifi::minifi::processors::InvokeHTTP>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<
      org::apache::nifi::minifi::processors::ListenHTTP>("listenhttp");

  std::shared_ptr<core::Processor> invokehttp = std::make_shared<
      org::apache::nifi::minifi::processors::InvokeHTTP>("invokehttp");
  uuid_t processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));

  uuid_t invokehttp_uuid;
  REQUIRE(true == invokehttp->getUUID(invokehttp_uuid));

  std::shared_ptr<minifi::Connection> connection = std::make_shared<
      minifi::Connection>(repo, "getfileCreate2Connection");
  connection->setRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> connection2 = std::make_shared<
      minifi::Connection>(repo, "listenhttp");

  connection2->setRelationship(core::Relationship("No Retry", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(processor);

  // link the connections so that we can test results at the end for this
  connection->setDestination(invokehttp);

  connection2->setSource(invokehttp);

  connection2->setSourceUUID(invokehttp_uuid);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(invokehttp_uuid);

  processor->addConnection(connection);
  invokehttp->addConnection(connection);
  invokehttp->addConnection(connection2);

  core::ProcessorNode node(processor);
  core::ProcessorNode node2(invokehttp);

  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider =
      nullptr;
  core::ProcessContext context(node, controller_services_provider, repo);
  core::ProcessContext context2(node2, controller_services_provider, repo);
  context.setProperty(org::apache::nifi::minifi::processors::ListenHTTP::Port,
                      "8685");
  context.setProperty(
      org::apache::nifi::minifi::processors::ListenHTTP::BasePath,
      "/testytesttest");

  context2.setProperty(
      org::apache::nifi::minifi::processors::InvokeHTTP::Method, "POST");
  context2.setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::URL,
                       "http://localhost:8685/testytesttest");
  core::ProcessSession session(&context);
  core::ProcessSession session2(&context2);

  REQUIRE(processor->getName() == "listenhttp");

  core::ProcessSessionFactory factory(&context);

  std::shared_ptr<core::FlowFile> record;
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onSchedule(&context, &factory);
  processor->onTrigger(&context, &session);

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  core::ProcessSessionFactory factory2(&context2);
  invokehttp->onSchedule(&context2, &factory2);
  invokehttp->onTrigger(&context2, &session2);

  provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
  std::set<provenance::ProvenanceEventRecord*> records = reporter->getEvents();
  record = session.get();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onTrigger(&context, &session);

  reporter = session.getProvenanceReporter();

  records = reporter->getEvents();
  session.commit();

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  invokehttp->onTrigger(&context2, &session2);

  session2.commit();
  records = reporter->getEvents();

  for (provenance::ProvenanceEventRecord *provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == processor->getName());
  }
  std::shared_ptr<core::FlowFile> ffr = session2.get();
  REQUIRE(true == LogTestController::getInstance().contains("exiting because method is POST"));
  LogTestController::getInstance().reset();
}

TEST_CASE("HTTPTestsWithNoResourceClaimPOST", "[httptest1]") {
  TestController testController;
  LogTestController::getInstance().setInfo<org::apache::nifi::minifi::processors::InvokeHTTP>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> getfileprocessor = std::make_shared<
      org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

  std::shared_ptr<core::Processor> logAttribute = std::make_shared<
      org::apache::nifi::minifi::processors::LogAttribute>("logattribute");

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  std::shared_ptr<core::Processor> listenhttp = std::make_shared<
      org::apache::nifi::minifi::processors::ListenHTTP>("listenhttp");

  std::shared_ptr<core::Processor> invokehttp = std::make_shared<
      org::apache::nifi::minifi::processors::InvokeHTTP>("invokehttp");
  uuid_t processoruuid;
  REQUIRE(true == listenhttp->getUUID(processoruuid));

  uuid_t invokehttp_uuid;
  REQUIRE(true == invokehttp->getUUID(invokehttp_uuid));

  std::shared_ptr<minifi::Connection> gcConnection = std::make_shared<
      minifi::Connection>(repo, "getfileCreate2Connection");
  gcConnection->setRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> laConnection = std::make_shared<
      minifi::Connection>(repo, "logattribute");
  laConnection->setRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> connection = std::make_shared<
      minifi::Connection>(repo, "getfileCreate2Connection");
  connection->setRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> connection2 = std::make_shared<
      minifi::Connection>(repo, "listenhttp");

  connection2->setRelationship(core::Relationship("No Retry", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(listenhttp);

  connection2->setSourceUUID(invokehttp_uuid);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(invokehttp_uuid);

  listenhttp->addConnection(connection);
  invokehttp->addConnection(connection);
  invokehttp->addConnection(connection2);

  core::ProcessorNode node(listenhttp);
  core::ProcessorNode node2(invokehttp);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider =
      nullptr;
  core::ProcessContext context(node, controller_services_provider, repo);
  core::ProcessContext context2(node2, controller_services_provider, repo);
  context.setProperty(org::apache::nifi::minifi::processors::ListenHTTP::Port,
                      "8686");
  context.setProperty(
      org::apache::nifi::minifi::processors::ListenHTTP::BasePath,
      "/testytesttest");

  context2.setProperty(
      org::apache::nifi::minifi::processors::InvokeHTTP::Method, "POST");
  context2.setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::URL,
                       "http://localhost:8686/testytesttest");
  core::ProcessSession session(&context);
  core::ProcessSession session2(&context2);

  REQUIRE(listenhttp->getName() == "listenhttp");

  core::ProcessSessionFactory factory(&context);

  std::shared_ptr<core::FlowFile> record;
  listenhttp->setScheduledState(core::ScheduledState::RUNNING);
  listenhttp->onSchedule(&context, &factory);
  listenhttp->onTrigger(&context, &session);

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  core::ProcessSessionFactory factory2(&context2);
  invokehttp->onSchedule(&context2, &factory2);
  invokehttp->onTrigger(&context2, &session2);

  provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
  std::set<provenance::ProvenanceEventRecord*> records = reporter->getEvents();
  record = session.get();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  listenhttp->incrementActiveTasks();
  listenhttp->setScheduledState(core::ScheduledState::RUNNING);
  listenhttp->onTrigger(&context, &session);

  reporter = session.getProvenanceReporter();

  records = reporter->getEvents();
  session.commit();

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  invokehttp->onTrigger(&context2, &session2);

  session2.commit();
  records = reporter->getEvents();

  for (provenance::ProvenanceEventRecord *provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == listenhttp->getName());
  }
  std::shared_ptr<core::FlowFile> ffr = session2.get();
  REQUIRE(true == LogTestController::getInstance().contains("exiting because method is POST"));
  LogTestController::getInstance().reset();
}

class CallBack : public minifi::OutputStreamCallback {
 public:
  CallBack() {
  }
  virtual ~CallBack() {
  }
  virtual void process(std::ofstream *stream) {
    std::string st = "we're gnna write some test stuff";
    stream->write(st.c_str(), st.length());
  }
};

TEST_CASE("HTTPTestsWithResourceClaimPOST", "[httptest1]") {
  TestController testController;
  LogTestController::getInstance().setInfo<org::apache::nifi::minifi::processors::InvokeHTTP>();

  std::shared_ptr<TestRepository> repo = std::make_shared<
      TestRepository>();

  std::shared_ptr<core::Processor> getfileprocessor = std::make_shared<
      org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

  std::shared_ptr<core::Processor> logAttribute = std::make_shared<
      org::apache::nifi::minifi::processors::LogAttribute>("logattribute");

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  std::shared_ptr<core::Processor> listenhttp = std::make_shared<
      org::apache::nifi::minifi::processors::ListenHTTP>("listenhttp");

  std::shared_ptr<core::Processor> invokehttp = std::make_shared<
      org::apache::nifi::minifi::processors::InvokeHTTP>("invokehttp");
  uuid_t processoruuid;
  REQUIRE(true == listenhttp->getUUID(processoruuid));

  uuid_t invokehttp_uuid;
  REQUIRE(true == invokehttp->getUUID(invokehttp_uuid));

  std::shared_ptr<minifi::Connection> gcConnection = std::make_shared<
      minifi::Connection>(repo, "getfileCreate2Connection");
  gcConnection->setRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> laConnection = std::make_shared<
      minifi::Connection>(repo, "logattribute");
  laConnection->setRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> connection = std::make_shared<
      minifi::Connection>(repo, "getfileCreate2Connection");
  connection->setRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> connection2 = std::make_shared<
      minifi::Connection>(repo, "listenhttp");

  connection2->setRelationship(core::Relationship("No Retry", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(listenhttp);

  connection->setSourceUUID(invokehttp_uuid);
  connection->setDestinationUUID(processoruuid);

  connection2->setSourceUUID(processoruuid);
  connection2->setSourceUUID(processoruuid);

  listenhttp->addConnection(connection);
  invokehttp->addConnection(connection);
  invokehttp->addConnection(connection2);

  core::ProcessorNode node(invokehttp);
  core::ProcessorNode node2(listenhttp);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider =
      nullptr;
  core::ProcessContext context(node, controller_services_provider, repo);
  core::ProcessContext context2(node2, controller_services_provider, repo);
  context.setProperty(org::apache::nifi::minifi::processors::ListenHTTP::Port,
                      "8680");
  context.setProperty(
      org::apache::nifi::minifi::processors::ListenHTTP::BasePath,
      "/testytesttest");

  context2.setProperty(
      org::apache::nifi::minifi::processors::InvokeHTTP::Method, "POST");
  context2.setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::URL,
                       "http://localhost:8680/testytesttest");
  core::ProcessSession session(&context);
  core::ProcessSession session2(&context2);

  REQUIRE(listenhttp->getName() == "listenhttp");

  core::ProcessSessionFactory factory(&context);

  std::shared_ptr<core::FlowFile> record;

  CallBack callback;

  /*
   explicit FlowFileRecord(std::shared_ptr<core::Repository> flow_repository,
   std::map<std::string, std::string> attributes,
   std::shared_ptr<ResourceClaim> claim = nullptr);
   */
  std::map<std::string, std::string> attributes;
  attributes["testy"] = "test";
  std::shared_ptr<minifi::FlowFileRecord> flow = std::make_shared<
      minifi::FlowFileRecord>(repo, attributes);
  session2.write(flow, &callback);

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  core::ProcessSessionFactory factory2(&context2);
  invokehttp->onSchedule(&context2, &factory2);
  invokehttp->onTrigger(&context2, &session2);

  listenhttp->incrementActiveTasks();
  listenhttp->setScheduledState(core::ScheduledState::RUNNING);
  listenhttp->onSchedule(&context, &factory);
  listenhttp->onTrigger(&context, &session);

  provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
  std::set<provenance::ProvenanceEventRecord*> records = reporter->getEvents();
  record = session.get();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  listenhttp->incrementActiveTasks();
  listenhttp->setScheduledState(core::ScheduledState::RUNNING);
  listenhttp->onTrigger(&context, &session);

  reporter = session.getProvenanceReporter();

  records = reporter->getEvents();
  session.commit();

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  invokehttp->onTrigger(&context2, &session2);

  session2.commit();
  records = reporter->getEvents();

  for (provenance::ProvenanceEventRecord *provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == listenhttp->getName());
  }
  std::shared_ptr<core::FlowFile> ffr = session2.get();
  REQUIRE(true == LogTestController::getInstance().contains("exiting because method is POST"));
  LogTestController::getInstance().reset();
}

