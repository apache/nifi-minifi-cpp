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

#include "TestBase.h"
#include "Catch.h"
#include "ConfigurationTestController.h"
#include "core/flow/AdaptiveConfiguration.h"
#include "utils/IntegrationTestUtils.h"

using namespace std::literals::chrono_literals;

TEST_CASE("Adaptive configuration can parse JSON") {
  ConfigurationTestController controller;

  const char* json_config = R"(
    {
      "Flow Controller": {"name": "root"},
      "Processors": [
        {
          "id": "00000000-0000-0000-0000-000000000001",
          "class": "DummyProcessor",
          "name": "Proc1"
        }
      ],
      "Connections": []
    }
  )";

  core::flow::AdaptiveConfiguration config{controller.getContext()};

  auto root = config.getRootFromPayload(json_config);

  REQUIRE(root->findProcessorByName("Proc1"));
}

TEST_CASE("Adaptive configuration can parse YAML") {
  ConfigurationTestController controller;

  const char* yaml_config = R"(
Flow Controller:
  name: root
Processors:
- id: 00000000-0000-0000-0000-000000000001
  class: DummyProcessor
  name: Proc1
Connections: []
  )";

  core::flow::AdaptiveConfiguration config{controller.getContext()};

  auto root = config.getRootFromPayload(yaml_config);

  REQUIRE(root->findProcessorByName("Proc1"));
}

TEST_CASE("Adaptive configuration logs json parse errors") {
  ConfigurationTestController controller;

  const char* json_config = R"(
    {
      "Flow Controller": {"name
      "Processors": [
        {
          "id": "00000000-0000-0000-0000-000000000001",
          "class": "DummyProcessor",
          "name": "Proc1"
        }
      ],
      "Connections": []
    }
  )";

  core::flow::AdaptiveConfiguration config{controller.getContext()};

  REQUIRE_THROWS(config.getRootFromPayload(json_config));

  REQUIRE(utils::verifyLogLinePresenceInPollTime(0s, "[debug] Could not parse configuration as json, trying yaml"));
  REQUIRE(utils::verifyLogLinePresenceInPollTime(0s, "[error] Configuration file is not valid json: Invalid encoding in string. (38)"));
  REQUIRE(utils::verifyLogLinePresenceInPollTime(0s, "[error] Configuration file is not valid yaml: yaml-cpp: error at line 3, column 27: end of map flow not found"));
}
