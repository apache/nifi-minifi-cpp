// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/ConfigurationTestController.h"
#include "core/flow/AdaptiveConfiguration.h"
#include "unit/TestUtils.h"

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

  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "[debug] Could not parse configuration as json, trying yaml"));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "[error] Configuration file is not valid json: Invalid encoding in string. (38)"));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "[error] Configuration file is not valid yaml: yaml-cpp: error at line 3, column 27: end of map flow not found"));
}
