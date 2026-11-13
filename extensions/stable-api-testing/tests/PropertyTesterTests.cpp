// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/flow/AdaptiveConfiguration.h"
#include "unit/Catch.h"
#include "unit/ConfigurationTestController.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"

using namespace std::literals::chrono_literals;

TEST_CASE("Required property without Properties Entry (adaptive yaml)") {
  const ConfigurationTestController controller;
  core::flow::AdaptiveConfiguration config{controller.getContext()};
  const auto config_yaml = R"(
MiNiFi Config Version: 3
Flow Controller:
  name: MiNiFi Flow
Processors:
  - name: My processor
    id: 00000000-0000-0000-0000-000000000001
    class: PropertyTester
Connections: [ ]
Remote Process Groups: [ ]
)";
  REQUIRE_THROWS(config.getRootFromPayload(config_yaml));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(100ms,
      "[error] Error while processing configuration file: Unable to parse configuration file for component named 'My processor' because required "
      "property 'RequiredPropertyWithoutDefaultValue' is not set"));
}

TEST_CASE("Required property without Properties Entry (adaptive json)") {
  const ConfigurationTestController controller;
  core::flow::AdaptiveConfiguration config{controller.getContext()};
  const auto config_json = R"(
{
  "Flow Controller": {"name": "root"},
  "Processors": [
    {
      "id": "00000000-0000-0000-0000-000000000001",
      "class": "PropertyTester",
      "name": "My processor"
    }
  ],
  "Connections": []
}
  )";
  REQUIRE_THROWS(config.getRootFromPayload(config_json));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(100ms,
      "[error] Error while processing configuration file: Unable to parse configuration file for component named 'My processor' because required "
      "property 'RequiredPropertyWithoutDefaultValue' is not set"));
}

TEST_CASE("Explicitly unsetting required property (adaptive yaml)") {
  const ConfigurationTestController controller;
  core::flow::AdaptiveConfiguration config{controller.getContext()};
  const auto config_yaml = R"(
MiNiFi Config Version: 3
Flow Controller:
  name: MiNiFi Flow
Processors:
  - name: My processor
    id: 00000000-0000-0000-0000-000000000001
    class: PropertyTester
    Properties:
      RequiredPropertyWithDefaultValue: ~
Connections: [ ]
Remote Process Groups: [ ]
)";
  REQUIRE_THROWS(config.getRootFromPayload(config_yaml));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(100ms,
      "[error] Error while processing configuration file: Unable to parse configuration file for component named 'My processor' because Can't "
      "explicitly unset required property"));
}

TEST_CASE("Explicitly unsetting required property (adaptive json)") {
  const ConfigurationTestController controller;
  core::flow::AdaptiveConfiguration config{controller.getContext()};
  const auto config_json = R"(
{
  "Flow Controller": {"name": "root"},
  "Processors": [
    {
      "id": "00000000-0000-0000-0000-000000000001",
      "class": "PropertyTester",
      "name": "My processor",
      "Properties": {
          "RequiredPropertyWithDefaultValue": null,
      }
    }
  ],
  "Connections": []
})";
  REQUIRE_THROWS(config.getRootFromPayload(config_json));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(100ms,
      "[error] Error while processing configuration file: Unable to parse configuration file for component named 'My processor' because Can't "
      "explicitly unset required property"));
}

TEST_CASE("Omitting optional property (adaptive yaml)") {
  const ConfigurationTestController controller;
  core::flow::AdaptiveConfiguration config{controller.getContext()};
  const auto config_yaml = R"(
MiNiFi Config Version: 3
Flow Controller:
  name: MiNiFi Flow
Processors:
  - name: My processor
    id: 00000000-0000-0000-0000-000000000001
    class: PropertyTester
    Properties:
      RequiredPropertyWithoutDefaultValue: 'bar'
Connections: [ ]
Remote Process Groups: [ ]
)";
  const auto root = config.getRootFromPayload(config_yaml);
  REQUIRE(root);
  const auto my_proc = root->findProcessorByName("My processor");
  REQUIRE(my_proc);
  const auto opt_val = my_proc->getProperty("OptionalPropertyWithDefaultValue");
  CHECK(opt_val == "default_val");
}

TEST_CASE("Omitting optional property (adaptive json)") {
  const ConfigurationTestController controller;
  core::flow::AdaptiveConfiguration config{controller.getContext()};
  const auto config_json = R"(
{
  "Flow Controller": {"name": "root"},
  "Processors": [
    {
      "id": "00000000-0000-0000-0000-000000000001",
      "class": "PropertyTester",
      "name": "My processor",
      "Properties": {
          "RequiredPropertyWithoutDefaultValue": "foo"
      }
    }
  ],
  "Connections": []
})";
  const auto root = config.getRootFromPayload(config_json);
  REQUIRE(root);
  const auto my_proc = root->findProcessorByName("My processor");
  REQUIRE(my_proc);
  const auto opt_val = my_proc->getProperty("OptionalPropertyWithDefaultValue");
  CHECK(opt_val == "default_val");
}

TEST_CASE("Explicitly unsetting optional property (adaptive yaml)") {
  const ConfigurationTestController controller;
  core::flow::AdaptiveConfiguration config{controller.getContext()};
  const auto config_yaml = R"(
MiNiFi Config Version: 3
Flow Controller:
  name: MiNiFi Flow
Processors:
  - name: My processor
    id: 00000000-0000-0000-0000-000000000001
    class: PropertyTester
    Properties:
      OptionalPropertyWithDefaultValue: ~
      RequiredPropertyWithoutDefaultValue: 'foo'
Connections: [ ]
Remote Process Groups: [ ]
)";
  const auto root = config.getRootFromPayload(config_yaml);
  REQUIRE(root);
  const auto my_proc = root->findProcessorByName("My processor");
  REQUIRE(my_proc);
  const auto opt_val = my_proc->getProperty("OptionalPropertyWithDefaultValue");
  CHECK(!opt_val);
  CHECK(!my_proc->getProperty("OptionalPropertyWithoutDefaultValue"));
  CHECK(my_proc->getProperty("RequiredPropertyWithDefaultValue") == "default_val");
  CHECK(my_proc->getProperty("RequiredPropertyWithoutDefaultValue") == "foo");
}

TEST_CASE("Explicitly unsetting optional property (adaptive json)") {
  const ConfigurationTestController controller;
  core::flow::AdaptiveConfiguration config{controller.getContext()};
  const auto config_json = R"(
{
  "Flow Controller": {"name": "root"},
  "Processors": [
    {
      "id": "00000000-0000-0000-0000-000000000001",
      "class": "PropertyTester",
      "name": "My processor",
      "Properties": {
          "OptionalPropertyWithDefaultValue": null,
          "RequiredPropertyWithoutDefaultValue": "foo"
      }
    }
  ],
  "Connections": []
})";
  const auto root = config.getRootFromPayload(config_json);
  REQUIRE(root);
  const auto my_proc = root->findProcessorByName("My processor");
  REQUIRE(my_proc);
  const auto opt_val = my_proc->getProperty("OptionalPropertyWithDefaultValue");
  CHECK(!opt_val);
  CHECK(!my_proc->getProperty("OptionalPropertyWithoutDefaultValue"));
  CHECK(my_proc->getProperty("RequiredPropertyWithDefaultValue") == "default_val");
  CHECK(my_proc->getProperty("RequiredPropertyWithoutDefaultValue") == "foo");
}
