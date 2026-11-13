// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <array>

#include "unit/TestBase.h"
#include "integration/TestServer.h"
#include "integration/HTTPHandlers.h"
#include "unit/Catch.h"
#include "core/Processor.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

TEST_CASE("TestAbsoluteTimeout", "[httptest]") {
  TestController controller;

  std::string port = "12324";
  std::string rootURI =  "/";
  TimeoutingHTTPHandler handler(std::vector<std::chrono::milliseconds>(35, 100ms));

  TestServer server(port, rootURI, &handler);

  auto plan = controller.createPlan();

  auto processor = plan->addProcessor("InvokeHTTP", "InvokeHTTP");
  REQUIRE(processor->setProperty("Read Timeout", "1 s"));
  REQUIRE(processor->setProperty("Remote URL", "http://localhost:" + port));
  processor->setAutoTerminatedRelationships(std::array{core::Relationship{"failure", "d"}});

  plan->runNextProcessor();

  REQUIRE(LogTestController::getInstance().contains("HTTP operation timed out, with absolute timeout 3000ms"));
}

}  // namespace org::apache::nifi::minifi::test
