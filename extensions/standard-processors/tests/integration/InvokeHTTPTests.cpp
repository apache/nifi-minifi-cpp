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
#include <array>
#include <memory>
#include <string>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/Core.h"
#include "http/HTTPClient.h"
#include "InvokeHTTP.h"
#include "processors/ListenHTTP.h"
#include "core/FlowFile.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "processors/LogAttribute.h"
#include "unit/SingleProcessorTestController.h"
#include "integration/ConnectionCountingServer.h"

namespace org::apache::nifi::minifi::test {

class TestHTTPServer {
 public:
  TestHTTPServer();
  static constexpr const char* PROCESSOR_NAME = "my_http_server";
  static constexpr const char* URL = "http://localhost:8681/testytesttest";

  void trigger() {
    LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::ListenHTTP>();
    LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::LogAttribute>();
    test_plan_->reset();
    test_controller_.runSession(test_plan_);
  }

 private:
  TestController test_controller_;
  std::shared_ptr<processors::ListenHTTP> listen_http_;
  std::shared_ptr<processors::LogAttribute> log_attribute_;
  std::shared_ptr<TestPlan> test_plan_ = test_controller_.createPlan();
};

TestHTTPServer::TestHTTPServer() {
  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::ListenHTTP>();
  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::LogAttribute>();

  listen_http_ = std::dynamic_pointer_cast<processors::ListenHTTP>(test_plan_->addProcessor("ListenHTTP", PROCESSOR_NAME));
  log_attribute_ = std::dynamic_pointer_cast<processors::LogAttribute>(test_plan_->addProcessor("LogAttribute", "LogAttribute", core::Relationship("success", "description"), true));
  REQUIRE(listen_http_);
  REQUIRE(log_attribute_);
  test_plan_->setProperty(listen_http_, org::apache::nifi::minifi::processors::ListenHTTP::BasePath, "testytesttest");
  test_plan_->setProperty(listen_http_, org::apache::nifi::minifi::processors::ListenHTTP::Port, "8681");
  test_plan_->setProperty(listen_http_, org::apache::nifi::minifi::processors::ListenHTTP::HeadersAsAttributesRegex, ".*");
  test_controller_.runSession(test_plan_);
}

TEST_CASE("HTTPTestsWithNoResourceClaimPOST", "[httptest1]") {
  TestController testController;
  TestHTTPServer http_server;

  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::InvokeHTTP>();

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> invokehttp = std::make_shared<org::apache::nifi::minifi::processors::InvokeHTTP>("invokehttp");
  invokehttp->initialize();

  minifi::utils::Identifier invokehttp_uuid = invokehttp->getUUID();
  REQUIRE(invokehttp_uuid);

  auto node = std::make_shared<core::ProcessorNodeImpl>(invokehttp.get());
  auto context = std::make_shared<core::ProcessContextImpl>(node, nullptr, repo, repo, content_repo);

  context->setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::Method, "POST");
  context->setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::URL, TestHTTPServer::URL);

  auto session = std::make_shared<core::ProcessSessionImpl>(context);

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  auto factory2 = std::make_shared<core::ProcessSessionFactoryImpl>(context);
  invokehttp->onSchedule(*context, *factory2);
  invokehttp->onTrigger(*context, *session);

  auto reporter = session->getProvenanceReporter();
  auto records = reporter->getEvents();
  auto record = session->get();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  reporter = session->getProvenanceReporter();

  records = reporter->getEvents();
  session->commit();

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  invokehttp->onTrigger(*context, *session);

  session->commit();
  records = reporter->getEvents();
  // FIXME(fgerlits): this test is very weak, as `records` is empty
  for (const auto& provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == TestHTTPServer::PROCESSOR_NAME);
  }

  REQUIRE(LogTestController::getInstance().contains("Exiting because method is POST"));
}

TEST_CASE("HTTPTestsWithResourceClaimPOST", "[httptest1]") {
  TestController testController;

  LogTestController::getInstance().setDebug<org::apache::nifi::minifi::processors::InvokeHTTP>();

  auto repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> listenhttp = std::make_shared<org::apache::nifi::minifi::processors::ListenHTTP>("listenhttp");
  listenhttp->initialize();

  std::shared_ptr<core::Processor> invokehttp = std::make_shared<org::apache::nifi::minifi::processors::InvokeHTTP>("invokehttp");
  invokehttp->initialize();

  minifi::utils::Identifier processoruuid = listenhttp->getUUID();
  REQUIRE(processoruuid);

  minifi::utils::Identifier invokehttp_uuid = invokehttp->getUUID();
  REQUIRE(invokehttp_uuid);

  auto configuration = std::make_shared<minifi::ConfigureImpl>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(configuration);

  auto connection = std::make_shared<minifi::ConnectionImpl>(repo, content_repo, "getfileCreate2Connection");
  connection->addRelationship(core::Relationship("success", "description"));

  auto connection2 = std::make_shared<minifi::ConnectionImpl>(repo, content_repo, "listenhttp");

  connection2->addRelationship(core::Relationship("No Retry", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(listenhttp.get());
  connection->setSourceUUID(invokehttp_uuid);
  connection->setDestinationUUID(processoruuid);
  connection2->setSourceUUID(processoruuid);

  listenhttp->addConnection(connection.get());
  invokehttp->addConnection(connection.get());
  invokehttp->addConnection(connection2.get());

  auto node = std::make_shared<core::ProcessorNodeImpl>(listenhttp.get());
  auto node2 = std::make_shared<core::ProcessorNodeImpl>(invokehttp.get());
  auto context = std::make_shared<core::ProcessContextImpl>(node, nullptr, repo, repo, content_repo);
  auto context2 = std::make_shared<core::ProcessContextImpl>(node2, nullptr, repo, repo, content_repo);
  context->setProperty(org::apache::nifi::minifi::processors::ListenHTTP::Port, "8680");
  context->setProperty(org::apache::nifi::minifi::processors::ListenHTTP::BasePath, "/testytesttest");

  context2->setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::Method, "POST");
  context2->setProperty(org::apache::nifi::minifi::processors::InvokeHTTP::URL, "http://localhost:8680/testytesttest");
  auto session = std::make_shared<core::ProcessSessionImpl>(context);
  auto session2 = std::make_shared<core::ProcessSessionImpl>(context2);

  REQUIRE(listenhttp->getName() == "listenhttp");

  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context);

  std::shared_ptr<core::FlowFile> record;

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  auto factory2 = std::make_shared<core::ProcessSessionFactoryImpl>(context2);
  invokehttp->onSchedule(*context2, *factory2);
  invokehttp->onTrigger(*context2, *session2);

  listenhttp->incrementActiveTasks();
  listenhttp->setScheduledState(core::ScheduledState::RUNNING);
  listenhttp->onSchedule(*context, *factory);
  listenhttp->onTrigger(*context, *session);

  auto reporter = session->getProvenanceReporter();
  auto records = reporter->getEvents();
  record = session->get();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  listenhttp->incrementActiveTasks();
  listenhttp->setScheduledState(core::ScheduledState::RUNNING);
  listenhttp->onTrigger(*context, *session);

  reporter = session->getProvenanceReporter();

  records = reporter->getEvents();
  session->commit();

  invokehttp->incrementActiveTasks();
  invokehttp->setScheduledState(core::ScheduledState::RUNNING);
  invokehttp->onTrigger(*context2, *session2);

  session2->commit();
  records = reporter->getEvents();
  // FIXME(fgerlits): this test is very weak, as `records` is empty
  for (const auto& provEventRecord : records) {
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

  plan->setProperty(invokehttp, org::apache::nifi::minifi::processors::InvokeHTTP::Method, "POST");
  plan->setProperty(invokehttp, org::apache::nifi::minifi::processors::InvokeHTTP::URL, TestHTTPServer::URL);
  testController.runSession(plan);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  plan->reset();
  testController.runSession(plan);

  records = plan->getProvenanceRecords();
  // FIXME(fgerlits): this test is very weak, as `records` is empty
  for (const auto& provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == TestHTTPServer::PROCESSOR_NAME);
  }

  REQUIRE(true == LogTestController::getInstance().contains("Exiting because method is POST"));
}

TEST_CASE("HTTPTestsPenalizeNoRetry", "[httptest1]") {
  using minifi::processors::InvokeHTTP;

  TestController testController;
  TestHTTPServer http_server;

  LogTestController::getInstance().setInfo<minifi::core::ProcessSession>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");
  std::shared_ptr<core::Processor> invokehttp = plan->addProcessor("InvokeHTTP", "invokehttp", core::Relationship("success", "description"), true);

  plan->setProperty(invokehttp, InvokeHTTP::Method, "GET");
  plan->setProperty(invokehttp, InvokeHTTP::URL, "http://localhost:8681/invalid");
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelFailure, InvokeHTTP::RelNoRetry, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});

  constexpr const char* PENALIZE_LOG_PATTERN = "Penalizing [0-9a-f-]+ for [0-9]+ms at invokehttp";

  SECTION("with penalize on no retry set to true") {
    plan->setProperty(invokehttp, InvokeHTTP::PenalizeOnNoRetry, "true");
    testController.runSession(plan);
    REQUIRE(LogTestController::getInstance().matchesRegex(PENALIZE_LOG_PATTERN));
  }

  SECTION("with penalize on no retry set to false") {
    plan->setProperty(invokehttp, InvokeHTTP::PenalizeOnNoRetry, "false");
    testController.runSession(plan);
    REQUIRE_FALSE(LogTestController::getInstance().matchesRegex(PENALIZE_LOG_PATTERN));
  }
}

TEST_CASE("InvokeHTTP fails with when flow contains invalid attribute names in HTTP headers", "[httptest1]") {
  using minifi::processors::InvokeHTTP;
  TestHTTPServer http_server;

  LogTestController::getInstance().setDebug<InvokeHTTP>();
  auto invokehttp = std::make_shared<InvokeHTTP>("InvokeHTTP");
  test::SingleProcessorTestController test_controller{invokehttp};

  invokehttp->setProperty(InvokeHTTP::Method, "GET");
  invokehttp->setProperty(InvokeHTTP::URL, TestHTTPServer::URL);
  invokehttp->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy, "fail");
  invokehttp->setProperty(InvokeHTTP::AttributesToSend, ".*");
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::Success, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"invalid header", "value"}});
  auto file_contents = result.at(InvokeHTTP::RelFailure);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
}

TEST_CASE("InvokeHTTP succeeds when the flow file contains an attribute that would be invalid as an HTTP header, and the policy is FAIL, but the attribute is not matched",
    "[httptest1][invokehttp][httpheader][attribute]") {
  using minifi::processors::InvokeHTTP;
  TestHTTPServer http_server;

  LogTestController::getInstance().setDebug<InvokeHTTP>();
  auto invokehttp = std::make_shared<InvokeHTTP>("InvokeHTTP");
  test::SingleProcessorTestController test_controller{invokehttp};

  invokehttp->setProperty(InvokeHTTP::Method, "GET");
  invokehttp->setProperty(InvokeHTTP::URL, TestHTTPServer::URL);
  invokehttp->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy, "fail");
  invokehttp->setProperty(InvokeHTTP::AttributesToSend, "valid.*");
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::Success, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"invalid header", "value"}, {"valid-header", "value2"}});
  REQUIRE(result.at(InvokeHTTP::RelFailure).empty());
  const auto& success_contents = result.at(InvokeHTTP::Success);
  REQUIRE(success_contents.size() == 1);
  http_server.trigger();
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:invalid"));
  REQUIRE(LogTestController::getInstance().contains("key:valid-header value:value2"));
}

TEST_CASE("InvokeHTTP replaces invalid characters of attributes", "[httptest1]") {
  using minifi::processors::InvokeHTTP;
  TestHTTPServer http_server;

  auto invokehttp = std::make_shared<InvokeHTTP>("InvokeHTTP");
  test::SingleProcessorTestController test_controller{invokehttp};
  LogTestController::getInstance().setTrace<InvokeHTTP>();

  invokehttp->setProperty(InvokeHTTP::Method, "GET");
  invokehttp->setProperty(InvokeHTTP::URL, TestHTTPServer::URL);
  invokehttp->setProperty(InvokeHTTP::AttributesToSend, ".*");
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::RelFailure, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"invalid header", "value"}, {"", "value2"}});
  auto file_contents = result.at(InvokeHTTP::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
  http_server.trigger();
  REQUIRE(LogTestController::getInstance().contains("key:invalid-header value:value"));
  REQUIRE(LogTestController::getInstance().contains("key:X-MiNiFi-Empty-Attribute-Name value:value2"));
}

TEST_CASE("InvokeHTTP drops invalid attributes from HTTP headers", "[httptest1]") {
  using minifi::processors::InvokeHTTP;
  TestHTTPServer http_server;

  auto invokehttp = std::make_shared<InvokeHTTP>("InvokeHTTP");
  test::SingleProcessorTestController test_controller{invokehttp};
  LogTestController::getInstance().setTrace<InvokeHTTP>();

  invokehttp->setProperty(InvokeHTTP::Method, "GET");
  invokehttp->setProperty(InvokeHTTP::URL, TestHTTPServer::URL);
  invokehttp->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy, "drop");
  invokehttp->setProperty(InvokeHTTP::AttributesToSend, ".*");
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::RelFailure, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"legit-header", "value1"}, {"invalid header", "value2"}});
  auto file_contents = result.at(InvokeHTTP::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
  http_server.trigger();
  REQUIRE(LogTestController::getInstance().contains("key:legit-header value:value1"));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:invalid", 0s));
}

TEST_CASE("InvokeHTTP empty Attributes to Send means no attributes are sent", "[httptest1]") {
  using minifi::processors::InvokeHTTP;
  TestHTTPServer http_server;

  auto invokehttp = std::make_shared<InvokeHTTP>("InvokeHTTP");
  test::SingleProcessorTestController test_controller{invokehttp};
  LogTestController::getInstance().setTrace<InvokeHTTP>();

  invokehttp->setProperty(InvokeHTTP::Method, "GET");
  invokehttp->setProperty(InvokeHTTP::URL, TestHTTPServer::URL);
  invokehttp->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy, "drop");
  invokehttp->setProperty(InvokeHTTP::AttributesToSend, "");
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::RelFailure, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"legit-header", "value1"}, {"invalid header", "value2"}});
  auto file_contents = result.at(InvokeHTTP::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
  http_server.trigger();
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:legit-header value:value1"));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:invalid", 0s));
}

TEST_CASE("InvokeHTTP DateHeader", "[InvokeHTTP]") {
  using minifi::processors::InvokeHTTP;
  TestHTTPServer http_server;

  auto invoke_http = std::make_shared<InvokeHTTP>("InvokeHTTP");
  test::SingleProcessorTestController test_controller{invoke_http};
  LogTestController::getInstance().setTrace<InvokeHTTP>();

  invoke_http->setProperty(InvokeHTTP::Method, "GET");
  invoke_http->setProperty(InvokeHTTP::URL, TestHTTPServer::URL);
  invoke_http->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy, "drop");
  invoke_http->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::RelFailure, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});

  bool date_header{};
  SECTION("DateHeader false") {
    date_header = false;
  };
  SECTION("DateHeader true") {
    date_header = true;
  };

  invoke_http->setProperty(InvokeHTTP::DateHeader, date_header ? "true" : "false");
  const auto result = test_controller.trigger("data");
  auto file_contents = result.at(InvokeHTTP::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
  http_server.trigger();
  REQUIRE(LogTestController::getInstance().contains("key:Date", 0ms) == date_header);
}

TEST_CASE("InvokeHTTP Attributes to Send uses full string matching, not substring", "[httptest1]") {
  using minifi::processors::InvokeHTTP;
  TestHTTPServer http_server;

  auto invokehttp = std::make_shared<InvokeHTTP>("InvokeHTTP");
  test::SingleProcessorTestController test_controller{invokehttp};
  LogTestController::getInstance().setTrace<InvokeHTTP>();

  invokehttp->setProperty(InvokeHTTP::Method, "GET");
  invokehttp->setProperty(InvokeHTTP::URL, TestHTTPServer::URL);
  invokehttp->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy, "drop");
  invokehttp->setProperty(InvokeHTTP::AttributesToSend, "he.*er");
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::RelFailure, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"header1", "value1"}, {"header", "value2"}});
  auto file_contents = result.at(InvokeHTTP::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
  http_server.trigger();
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:header1 value:value1"));
  REQUIRE(LogTestController::getInstance().contains("key:header value:value2"));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:invalid", 0s));
}

TEST_CASE("HTTPTestsResponseBodyinAttribute", "[InvokeHTTP]") {
  using minifi::processors::InvokeHTTP;

  auto invoke_http = std::make_shared<InvokeHTTP>("InvokeHTTP");
  test::SingleProcessorTestController test_controller{invoke_http};

  minifi::test::ConnectionCountingServer connection_counting_server;

  invoke_http->setProperty(InvokeHTTP::Method, "POST");
  invoke_http->setProperty(InvokeHTTP::URL, "http://localhost:" + connection_counting_server.getPort()  + "/reverse");
  invoke_http->setProperty(InvokeHTTP::PutResponseBodyInAttribute, "http.body");
  const auto result = test_controller.trigger("data", {{"header1", "value1"}, {"header", "value2"}});
  auto success_flow_files = result.at(InvokeHTTP::Success);
  CHECK(result.at(InvokeHTTP::RelFailure).empty());
  CHECK(result.at(InvokeHTTP::RelResponse).empty());
  CHECK(result.at(InvokeHTTP::RelNoRetry).empty());
  CHECK(result.at(InvokeHTTP::RelRetry).empty());
  REQUIRE(success_flow_files.size() == 1);
  CHECK(test_controller.plan->getContent(success_flow_files[0]) == "data");

  auto http_type_attribute = success_flow_files[0]->getAttribute("http.body");
  REQUIRE(http_type_attribute);
  CHECK(*http_type_attribute == "atad");
}

TEST_CASE("HTTPTestsResponseBody", "[InvokeHTTP]") {
  using minifi::processors::InvokeHTTP;

  auto invoke_http = std::make_shared<InvokeHTTP>("InvokeHTTP");
  test::SingleProcessorTestController test_controller{invoke_http};

  minifi::test::ConnectionCountingServer connection_counting_server;

  invoke_http->setProperty(InvokeHTTP::Method, "POST");
  invoke_http->setProperty(InvokeHTTP::URL, "http://localhost:" + connection_counting_server.getPort()  + "/reverse");
  invoke_http->setProperty(InvokeHTTP::SendMessageBody, "true");
  const auto result = test_controller.trigger("data", {{"header1", "value1"}, {"header", "value2"}});
  CHECK(result.at(InvokeHTTP::RelFailure).empty());
  CHECK(result.at(InvokeHTTP::RelNoRetry).empty());
  CHECK(result.at(InvokeHTTP::RelRetry).empty());
  CHECK(!result.at(InvokeHTTP::Success).empty());
  CHECK(!result.at(InvokeHTTP::RelResponse).empty());

  auto success_flow_files = result.at(InvokeHTTP::RelResponse);
  REQUIRE(success_flow_files.size() == 1);
  REQUIRE(test_controller.plan->getContent(success_flow_files[0]) == "atad");
}

TEST_CASE("Test Keepalive", "[InvokeHTTP]") {
  using minifi::processors::InvokeHTTP;

  auto invoke_http = std::make_shared<InvokeHTTP>("InvokeHTTP");
  test::SingleProcessorTestController test_controller{invoke_http};

  minifi::test::ConnectionCountingServer connection_counting_server;

  invoke_http->setProperty(InvokeHTTP::Method, "GET");
  invoke_http->setProperty(InvokeHTTP::URL, "http://localhost:" + connection_counting_server.getPort()  + "/method");

  for (auto i = 0; i < 4; ++i) {
    const auto result = test_controller.trigger(InputFlowFileData{"data"});
    CHECK(result.at(InvokeHTTP::RelFailure).empty());
    CHECK(result.at(InvokeHTTP::RelNoRetry).empty());
    CHECK(result.at(InvokeHTTP::RelRetry).empty());
    CHECK(!result.at(InvokeHTTP::Success).empty());
    CHECK(!result.at(InvokeHTTP::RelResponse).empty());
  }

  CHECK(1 == connection_counting_server.getConnectionCounter());
}

}  // namespace org::apache::nifi::minifi::test
