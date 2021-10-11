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
#include "processors/GenerateFlowFile.h"

namespace {
class TestHTTPServer {
 public:
  TestHTTPServer();
  static constexpr const char* PROCESSOR_NAME = "my_http_server";
  static constexpr const char* URL = "http://localhost:8681/testytesttest";

 private:
  TestController test_controller_;
  std::shared_ptr<TestPlan> test_plan_ = test_controller_.createPlan();
};

TestHTTPServer::TestHTTPServer() {
  std::shared_ptr<core::Processor> listen_http = test_plan_->addProcessor("ListenHTTP", PROCESSOR_NAME);
  test_plan_->setProperty(listen_http, org::apache::nifi::minifi::processors::ListenHTTP::BasePath.getName(), "/testytesttest");
  test_plan_->setProperty(listen_http, org::apache::nifi::minifi::processors::ListenHTTP::Port.getName(), "8681");
  test_controller_.runSession(test_plan_);
}
}  // namespace

TEST_CASE("HTTPTestsWithNoResourceClaimPOST", "[httptest1]") {
  TestController testController;
  TestHTTPServer http_server;

  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::InvokeHTTP>();

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> invokehttp = std::make_shared<org::apache::nifi::minifi::processors::InvokeHTTP>("invokehttp");
  invokehttp->initialize();

  utils::Identifier invokehttp_uuid = invokehttp->getUUID();
  REQUIRE(invokehttp_uuid);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(invokehttp);
  std::shared_ptr<core::ProcessContext> context = std::make_shared<core::ProcessContext>(node, nullptr, repo, repo, content_repo);

  context->setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::Method, "POST");
  context->setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::URL, TestHTTPServer::URL);

  auto session = std::make_shared<core::ProcessSession>(context);

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  std::shared_ptr<core::ProcessSessionFactory> factory2 = std::make_shared<core::ProcessSessionFactory>(context);
  invokehttp->onSchedule(context, factory2);
  invokehttp->onTrigger(context, session);

  auto reporter = session->getProvenanceReporter();
  auto records = reporter->getEvents();
  std::shared_ptr<core::FlowFile> record = session->get();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  reporter = session->getProvenanceReporter();

  records = reporter->getEvents();
  session->commit();

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  invokehttp->onTrigger(context, session);

  session->commit();
  records = reporter->getEvents();
  // FIXME(fgerlits): this test is very weak, as `records` is empty
  for (auto provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == TestHTTPServer::PROCESSOR_NAME);
  }

  REQUIRE(LogTestController::getInstance().contains("Exiting because method is POST"));
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

  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::InvokeHTTP>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

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
  // FIXME(fgerlits): this test is very weak, as `records` is empty
  for (auto provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == listenhttp->getName());
  }

  REQUIRE(true == LogTestController::getInstance().contains("Exiting because method is POST"));
}

TEST_CASE("HTTPTestsPostNoResourceClaim", "[httptest1]") {
  TestController testController;
  TestHTTPServer http_server;

  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::InvokeHTTP>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> invokehttp = plan->addProcessor("InvokeHTTP", "invokehttp");

  plan->setProperty(invokehttp, org::apache::nifi::minifi::processors::InvokeHTTP::Method.getName(), "POST");
  plan->setProperty(invokehttp, org::apache::nifi::minifi::processors::InvokeHTTP::URL.getName(), TestHTTPServer::URL);
  testController.runSession(plan);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  plan->reset();
  testController.runSession(plan);

  records = plan->getProvenanceRecords();
  // FIXME(fgerlits): this test is very weak, as `records` is empty
  for (auto provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == TestHTTPServer::PROCESSOR_NAME);
  }

  REQUIRE(true == LogTestController::getInstance().contains("Exiting because method is POST"));
}

TEST_CASE("HTTPTestsPenalizeNoRetry", "[httptest1]") {
  using processors::InvokeHTTP;

  TestController testController;
  TestHTTPServer http_server;

  LogTestController::getInstance().setInfo<minifi::core::ProcessSession>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");
  std::shared_ptr<core::Processor> invokehttp = plan->addProcessor("InvokeHTTP", "invokehttp", core::Relationship("success", "description"), true);

  plan->setProperty(invokehttp, InvokeHTTP::Method.getName(), "GET");
  plan->setProperty(invokehttp, InvokeHTTP::URL.getName(), TestHTTPServer::URL);
  invokehttp->setAutoTerminatedRelationships({InvokeHTTP::RelFailure, InvokeHTTP::RelNoRetry, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});

  constexpr const char* PENALIZE_LOG_PATTERN = "Penalizing [0-9a-f-]+ for [0-9]+ms at invokehttp";

  SECTION("with penalize on no retry set to true") {
    plan->setProperty(invokehttp, InvokeHTTP::PenalizeOnNoRetry.getName(), "true");
    testController.runSession(plan);
    REQUIRE(LogTestController::getInstance().matchesRegex(PENALIZE_LOG_PATTERN));
  }

  SECTION("with penalize on no retry set to false") {
    plan->setProperty(invokehttp, InvokeHTTP::PenalizeOnNoRetry.getName(), "false");
    testController.runSession(plan);
    REQUIRE_FALSE(LogTestController::getInstance().matchesRegex(PENALIZE_LOG_PATTERN));
  }
}

TEST_CASE("HTTPTestsPutResponseBodyinAttribute", "[httptest1]") {
  using processors::InvokeHTTP;

  TestController testController;
  TestHTTPServer http_server;

  LogTestController::getInstance().setDebug<InvokeHTTP>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");
  std::shared_ptr<core::Processor> invokehttp = plan->addProcessor("InvokeHTTP", "invokehttp", core::Relationship("success", "description"), true);

  plan->setProperty(invokehttp, InvokeHTTP::Method.getName(), "GET");
  plan->setProperty(invokehttp, InvokeHTTP::URL.getName(), TestHTTPServer::URL);
  plan->setProperty(invokehttp, InvokeHTTP::PropPutOutputAttributes.getName(), "http.type");
  invokehttp->setAutoTerminatedRelationships({InvokeHTTP::RelFailure, InvokeHTTP::RelNoRetry, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  testController.runSession(plan);

  REQUIRE(LogTestController::getInstance().contains("Adding http response body to flow file attribute http.type"));
}
