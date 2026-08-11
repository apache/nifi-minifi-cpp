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
}
