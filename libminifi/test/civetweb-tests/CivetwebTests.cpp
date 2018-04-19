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
#include <set>
#include <iostream>
#include <GenerateFlowFile.h>
#include <UpdateAttribute.h>
#include <LogAttribute.h>
#include <processors/ListenHTTP.h>

#include "../TestBase.h"

#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "../../../extensions/http-curl/client/HTTPClient.h"

TEST_CASE("Test Creation of ListenHTTP", "[ListenHTTPreate]") {  // NOLINT
  TestController testController;
  std::shared_ptr<core::Processor>
      processor = std::make_shared<org::apache::nifi::minifi::processors::ListenHTTP>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE("Test GET Body", "[ListenHTTPGETBody]") {  // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::GenerateFlowFile>();
  LogTestController::getInstance().setTrace<processors::UpdateAttribute>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  LogTestController::getInstance().setTrace<processors::ListenHTTP>();
  LogTestController::getInstance().setTrace<processors::ListenHTTP::Handler>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Define directory for test input
  std::string test_in_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&test_in_dir[0]) != nullptr);

  // Define test input file
  std::string test_input_file(test_in_dir);
  test_input_file.append("/test");
  {
    std::ofstream os(test_input_file);
    os << "Hello response body" << std::endl;
  }

  // Build MiNiFi processing graph
  auto get = plan->addProcessor(
      "GetFile",
      "Get");
  plan->setProperty(
      get,
      "Input Directory",
      test_in_dir);
  auto update = plan->addProcessor(
      "UpdateAttribute",
      "Update",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      update,
      "http.type",
      "response_body",
      true);
  auto log = plan->addProcessor(
      "LogAttribute",
      "Log",
      core::Relationship("success", "description"),
      true);
  auto listen = plan->addProcessor(
      "ListenHTTP",
      "ListenHTTP",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      listen,
      "Listening Port",
      "8888");
  listen->setAutoTerminatedRelationships({{"success", ""}});

  plan->runNextProcessor();  // Get
  plan->runNextProcessor();  // Update
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // Listen

  sleep(1);
  utils::HTTPClient client("http://localhost:8888/contentListener/test");
  REQUIRE(client.submit());
  const auto &body_chars = client.getResponseBody();
  std::string response_body(body_chars.data(), body_chars.size());
  REQUIRE("Hello response body\n" == response_body);
}
