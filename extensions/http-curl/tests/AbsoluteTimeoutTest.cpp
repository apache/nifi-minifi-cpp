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
#undef NDEBUG
#include <array>

#include "TestBase.h"
#include "tests/TestServer.h"
#include "HTTPHandlers.h"

using namespace std::literals::chrono_literals;

int main() {
  TestController controller;

  std::string port = "12324";
  std::string rootURI =  "/";
  TimeoutingHTTPHandler handler(std::vector<std::chrono::milliseconds>(35, 100ms));

  TestServer server(port, rootURI, &handler);

  auto plan = controller.createPlan();

  auto processor = plan->addProcessor("InvokeHTTP", "InvokeHTTP");
  processor->setProperty("Read Timeout", "1 s");
  processor->setProperty("Remote URL", "http://localhost:" + port);
  processor->setAutoTerminatedRelationships(std::array{core::Relationship{"failure", "d"}});

  plan->runNextProcessor();

  assert(LogTestController::getInstance().contains("HTTP operation timed out, with absolute timeout 3000ms"));
}
