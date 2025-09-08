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

#include <map>
#include <memory>
#include <chrono>
#include "core/repository/VolatileContentRepository.h"
#include "core/ProcessGroup.h"
#include "core/RepositoryFactory.h"
#include "core/yaml/YamlConfiguration.h"
#include "TailFile.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/StringUtils.h"
#include "unit/ConfigurationTestController.h"
#include "unit/TestUtils.h"
#include "core/Resource.h"
#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"
#include "unit/DummyProcessor.h"
#include "catch2/generators/catch_generators.hpp"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

TEST_CASE("Test YAML Config Processing", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());

  SECTION("loading YAML without optional component IDs works") {
    static const std::string CONFIG_YAML_WITHOUT_IDS =
      R"(
MiNiFi Config Version: 1
Flow Controller:
    name: MiNiFi Flow
    comment:

Core Properties:
    flow controller graceful shutdown period: 10 sec
    flow service write delay interval: 500 ms
    administrative yield duration: 30 sec
    bored yield duration: 10 millis

FlowFile Repository:
    partitions: 256
    checkpoint interval: 2 mins
    always sync: false
    Swap:
        threshold: 20000
        in period: 5 sec
        in threads: 1
        out period: 5 sec
        out threads: 4

Provenance Repository:
    provenance rollover time: 1 min

Content Repository:
    content claim max appendable size: 10 MB
    content claim max flow files: 100
    always sync: false

Component Status Repository:
    buffer size: 1440
    snapshot frequency: 1 min

Security Properties:
    keystore: /tmp/ssl/localhost-ks.jks
    keystore type: JKS
    keystore password: localtest
    key password: localtest
    truststore: /tmp/ssl/localhost-ts.jks
    truststore type: JKS
    truststore password: localtest
    ssl protocol: TLS
    Sensitive Props:
        key:
        algorithm: PBEWITHMD5AND256BITAES-CBC-OPENSSL
        provider: BC

Processors:
    - name: TailFile
      class: org.apache.nifi.processors.standard.TailFile
      max concurrent tasks: 1
      scheduling strategy: TIMER_DRIVEN
      scheduling period: 1 sec
      penalization period: 30 sec
      yield period: 1 sec
      bulletin level: ERROR
      run duration nanos: 0
      auto-terminated relationships list:
      Properties:
          File to Tail: logs/minifi-app.log
          Rolling Filename Pattern: minifi-app*
          Initial Start Position: Beginning of File

Connections:
    - name: TailToS2S
      source name: TailFile
      source relationship name: success
      destination name: 8644cbcc-a45c-40e0-964d-5e536e2ada61
      max work queue size: 0
      max work queue data size: 1 MB
      flowfile expiration: 60 sec
      queue prioritizer class: org.apache.nifi.prioritizer.NewestFlowFileFirstPrioritizer

Remote Processing Groups:
    - name: NiFi Flow
      comment:
      url: https://localhost:8090/nifi
      timeout: 30 secs
      yield period: 10 sec
      Input Ports:
          - id: 8644cbcc-a45c-40e0-964d-5e536e2ada61
            name: tailed log
            comments:
            max concurrent tasks: 1
            use compression: false

Provenance Reporting:
    comment:
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 30 sec
    host: localhost
    port name: provenance
    port: 8090
    port uuid: 2f389b8d-83f2-48d3-b465-048f28a1cb56
    url: https://localhost:8090/
    originating url: http://${hostname(true)}:8081/nifi
    use compression: true
    timeout: 30 secs
    batch size: 1000;
        )";

    std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getRootFromPayload(CONFIG_YAML_WITHOUT_IDS);

    REQUIRE(rootFlowConfig);
    REQUIRE(rootFlowConfig->findProcessorByName("TailFile"));
    minifi::utils::Identifier uuid = rootFlowConfig->findProcessorByName("TailFile")->getUUID();
    REQUIRE(uuid);
    REQUIRE(!rootFlowConfig->findProcessorByName("TailFile")->getUUIDStr().empty());
    REQUIRE(1 == rootFlowConfig->findProcessorByName("TailFile")->getMaxConcurrentTasks());
    REQUIRE(core::SchedulingStrategy::TIMER_DRIVEN == rootFlowConfig->findProcessorByName("TailFile")->getSchedulingStrategy());
    REQUIRE(1s == rootFlowConfig->findProcessorByName("TailFile")->getSchedulingPeriod());
    REQUIRE(30s == rootFlowConfig->findProcessorByName("TailFile")->getPenalizationPeriod());
    REQUIRE(1s == rootFlowConfig->findProcessorByName("TailFile")->getYieldPeriod());
    REQUIRE(rootFlowConfig->findProcessorByName("TailFile")->getLogBulletinLevel() == logging::LOG_LEVEL::err);
    REQUIRE(0s == rootFlowConfig->findProcessorByName("TailFile")->getRunDurationNano());

    std::map<std::string, minifi::Connection*> connectionMap;
    rootFlowConfig->getConnections(connectionMap);
    REQUIRE(2 == connectionMap.size());
    // This is a map of UUID->Connection, and we don't know UUID, so just going to loop over it
    for (const auto& it : connectionMap) {
      REQUIRE(it.second);
      REQUIRE(!it.second->getUUIDStr().empty());
      REQUIRE(it.second->getDestination());
      REQUIRE(it.second->getSource());
      REQUIRE(60s == it.second->getFlowExpirationDuration());
    }
  }

  SECTION("missing required field in YAML throws exception") {
    static const std::string CONFIG_YAML_NO_RPG_PORT_ID =
      R"(
MiNiFi Config Version: 1
Flow Controller:
  name: MiNiFi Flow
Processors: []
Connections: []
Remote Processing Groups:
    - name: NiFi Flow
      comment:
      url: https://localhost:8090/nifi
      timeout: 30 secs
      yield period: 10 sec
      Input Ports:
          - name: tailed log
            comments:
            max concurrent tasks: 1
            use compression: false
      )";

    REQUIRE_THROWS_AS(yamlConfig.getRootFromPayload(CONFIG_YAML_NO_RPG_PORT_ID), std::invalid_argument);
  }

  SECTION("Validated YAML property failure throws exception and logs invalid attribute name") {
    static const std::string CONFIG_YAML_EMPTY_RETRY_ATTRIBUTE =
      R"(
Flow Controller:
  name: MiNiFi Flow
Processors:
  - name: RetryFlowFile
    class: org.apache.nifi.processors.standard.RetryFlowFile
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 1 sec
    auto-terminated relationships list:
    Properties:
        Retry Attribute: ""
Connections: []
Remote Processing Groups: []
Provenance Reporting:
      )";

    REQUIRE_THROWS_WITH(yamlConfig.getRootFromPayload(CONFIG_YAML_EMPTY_RETRY_ATTRIBUTE), "Unable to parse configuration file for component named 'RetryFlowFile' because ValidationFailed");
    REQUIRE(LogTestController::getInstance().contains("Invalid value was set for property 'Retry Attribute' creating component 'RetryFlowFile'"));
  }
}

TEST_CASE("Test YAML v3 Invalid Type", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: Simple TailFile To RPG
  comment: ''
Core Properties:
  flow controller graceful shutdown period: 10 sec
  flow service write delay interval: 500 ms
  administrative yield duration: 30 sec
  bored yield duration: 10 millis
  max concurrent threads: 1
  variable registry properties: ''
FlowFile Repository:
  partitions: 256
  checkpoint interval: 2 mins
  always sync: false
  Swap:
    threshold: 20000
    in period: 5 sec
    in threads: 1
    out period: 5 sec
    out threads: 4
Content Repository:
  content claim max appendable size: 10 MB
  content claim max flow files: 100
  always sync: false
Provenance Repository:
  provenance rollover time: 1 min
  implementation: org.apache.nifi.provenance.MiNiFiPersistentProvenanceRepository
Component Status Repository:
  buffer size: 1440
  snapshot frequency: 1 min
Security Properties:
  keystore: ''
  keystore type: ''
  keystore password: ''
  key password: ''
  truststore: ''
  truststore type: ''
  truststore password: ''
  ssl protocol: ''
  Sensitive Props:
    key:
    algorithm: PBEWITHMD5AND256BITAES-CBC-OPENSSL
    provider: BC
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: TailFile
  class: org.apache.nifi.processors.standard.TailFile
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  penalization period: 30 sec
  yield period: 1 sec
  run duration nanos: 0
  auto-terminated relationships list: []
  Properties:
    File Location: Local
    File to Tail: ./logs/minifi-app.log
    Initial Start Position: Beginning of File
    Rolling Filename Pattern:
    tail-base-directory:
    tail-mode: Single file
    tailfile-lookup-frequency: 10 minutes
    tailfile-maximum-age: 24 hours
    tailfile-recursive-lookup: 'false'
    tailfile-rolling-strategy: Fixed name
Controller Services: []
Process Groups: []
Input Ports: []
Output Ports: []
Funnels: []
Connections:
- id: b0c0c3cc-0158-1000-0000-000000000000
  name: TailFile/success/ac0e798c-0158-1000-0588-cda9b944e011
  source id: b0c04f28-0158-1000-0000-000000000000
  source relationship names:
  - success
  destination id: ac0e798c-0158-1000-0588-cda9b944e011
  max work queue size: 10000
  max work queue data size: 1 GB
  flowfile expiration: 0 sec
  queue prioritizer class: ''
Remote Process Groups:
- id: b0c09ff0-0158-1000-0000-000000000000
  name: ''
  url: http://localhost:8080/nifi
  comment: ''
  timeout: 30 sec
  yield period: 10 sec
  transport protocol: WRONG
  proxy host: ''
  proxy port: ''
  proxy user: ''
  proxy password: ''
  local network interface: ''
  Input Ports:
  - id: aca664f8-0158-1000-a139-92485891d349
    name: test2
    comment: ''
    max concurrent tasks: 1
    use compression: false
  - id: ac0e798c-0158-1000-0588-cda9b944e011
    name: test
    comment: ''
    max concurrent tasks: 1
    use compression: false
  Output Ports: []
NiFi Properties Overrides: {}
      )";

  REQUIRE_THROWS_AS(yamlConfig.getRootFromPayload(TEST_CONFIG_YAML), minifi::Exception);
}

TEST_CASE("Test YAML v3 Config Processing", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: Simple TailFile To RPG
  comment: ''
Core Properties:
  flow controller graceful shutdown period: 10 sec
  flow service write delay interval: 500 ms
  administrative yield duration: 30 sec
  bored yield duration: 10 millis
  max concurrent threads: 1
  variable registry properties: ''
FlowFile Repository:
  partitions: 256
  checkpoint interval: 2 mins
  always sync: false
  Swap:
    threshold: 20000
    in period: 5 sec
    in threads: 1
    out period: 5 sec
    out threads: 4
Content Repository:
  content claim max appendable size: 10 MB
  content claim max flow files: 100
  always sync: false
Provenance Repository:
  provenance rollover time: 1 min
  implementation: org.apache.nifi.provenance.MiNiFiPersistentProvenanceRepository
Component Status Repository:
  buffer size: 1440
  snapshot frequency: 1 min
Security Properties:
  keystore: ''
  keystore type: ''
  keystore password: ''
  key password: ''
  truststore: ''
  truststore type: ''
  truststore password: ''
  ssl protocol: ''
  Sensitive Props:
    key:
    algorithm: PBEWITHMD5AND256BITAES-CBC-OPENSSL
    provider: BC
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: TailFile
  class: org.apache.nifi.processors.standard.TailFile
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  penalization period: 30 sec
  yield period: 1 sec
  run duration nanos: 0
  auto-terminated relationships list: []
  Properties:
    File Location: Local
    File to Tail: ./logs/minifi-app.log
    Initial Start Position: Beginning of File
    Rolling Filename Pattern:
    tail-base-directory:
    tail-mode: Single file
    tailfile-lookup-frequency: 10 minutes
    tailfile-maximum-age: 24 hours
    tailfile-recursive-lookup: 'false'
    tailfile-rolling-strategy: Fixed name
Controller Services: []
Process Groups: []
Input Ports: []
Output Ports: []
Funnels: []
Connections:
- id: b0c0c3cc-0158-1000-0000-000000000000
  name: TailFile/success/ac0e798c-0158-1000-0588-cda9b944e011
  source id: b0c04f28-0158-1000-0000-000000000000
  source relationship names:
  - success
  destination id: ac0e798c-0158-1000-0588-cda9b944e011
  max work queue size: 10000
  max work queue data size: 1 GB
  flowfile expiration: 0 sec
  queue prioritizer class: ''
Remote Process Groups:
- id: b0c09ff0-0158-1000-0000-000000000000
  name: ''
  url: http://localhost:8080/nifi
  comment: ''
  timeout: 20 sec
  yield period: 10 sec
  transport protocol: RAW
  proxy host: ''
  proxy port: ''
  proxy user: ''
  proxy password: ''
  local network interface: ''
  Input Ports:
  - id: ac0e798c-0158-1000-0588-cda9b944e011
    name: AmazingInputPort
    comment: ''
    max concurrent tasks: 1
    use compression: true
    batch size:
      size: 10 B
      count: 3
      duration: 5 sec
  Output Ports: []
NiFi Properties Overrides: {}
      )";
  std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getRootFromPayload(TEST_CONFIG_YAML);

  REQUIRE(rootFlowConfig);
  REQUIRE(rootFlowConfig->findProcessorByName("TailFile"));
  minifi::utils::Identifier uuid = rootFlowConfig->findProcessorByName("TailFile")->getUUID();
  REQUIRE(uuid);
  REQUIRE(!rootFlowConfig->findProcessorByName("TailFile")->getUUIDStr().empty());
  REQUIRE(1 == rootFlowConfig->findProcessorByName("TailFile")->getMaxConcurrentTasks());
  REQUIRE(core::SchedulingStrategy::TIMER_DRIVEN == rootFlowConfig->findProcessorByName("TailFile")->getSchedulingStrategy());
  REQUIRE(1 == rootFlowConfig->findProcessorByName("TailFile")->getMaxConcurrentTasks());
  REQUIRE(1s == rootFlowConfig->findProcessorByName("TailFile")->getSchedulingPeriod());
  REQUIRE(30s == rootFlowConfig->findProcessorByName("TailFile")->getPenalizationPeriod());
  REQUIRE(1s == rootFlowConfig->findProcessorByName("TailFile")->getYieldPeriod());
  REQUIRE(0s == rootFlowConfig->findProcessorByName("TailFile")->getRunDurationNano());

  std::map<std::string, minifi::Connection*> connectionMap;
  rootFlowConfig->getConnections(connectionMap);
  REQUIRE(2 == connectionMap.size());

  for (const auto& it : connectionMap) {
    REQUIRE(it.second);
    REQUIRE(!it.second->getUUIDStr().empty());
    REQUIRE(it.second->getDestination());
    REQUIRE(it.second->getSource());
    REQUIRE(0s == it.second->getFlowExpirationDuration());
  }

  TypedProcessorWrapper<minifi::RemoteProcessGroupPort> port =rootFlowConfig->findProcessorByName("AmazingInputPort");
  REQUIRE(port);
  CHECK(port->getUUIDStr() == "ac0e798c-0158-1000-0588-cda9b944e011");
  CHECK(port.get().getUseCompression() == true);
  CHECK(port.get().getBatchSize().value() == 10);
  CHECK(port.get().getBatchCount().value() == 3);
  CHECK(port.get().getBatchDuration().value() == std::chrono::seconds(5));
  CHECK(port.get().getTimeout().value() == std::chrono::seconds(20));
}

TEST_CASE("Test Dynamic Unsupported", "[YamlConfigurationDynamicUnsupported]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML = R"(
Flow Controller:
  name: Simple
Processors:
- name: GenerateFlowFile
  class: GenerateFlowFile
  Properties:
     Dynamic Property: Bad
      )";
  std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getRootFromPayload(TEST_CONFIG_YAML);

  REQUIRE(rootFlowConfig);
  REQUIRE(rootFlowConfig->findProcessorByName("GenerateFlowFile"));
  const minifi::utils::Identifier uuid = rootFlowConfig->findProcessorByName("GenerateFlowFile")->getUUID();
  REQUIRE(uuid);
  REQUIRE(!rootFlowConfig->findProcessorByName("GenerateFlowFile")->getUUIDStr().empty());

  REQUIRE(LogTestController::getInstance().contains("[warning] Unable to set the dynamic property Dynamic Property"));
}

TEST_CASE("Test Required Property", "[YamlConfigurationRequiredProperty]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML = R"(
Flow Controller:
  name: Simple
Processors:
- name: XYZ
  class: GetFile
  Properties:
    Input Directory: ""
    Batch Size: 1
      )";
  bool caught_exception = false;

  try {
    std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getRootFromPayload(TEST_CONFIG_YAML);

    REQUIRE(rootFlowConfig);
    REQUIRE(rootFlowConfig->findProcessorByName("GetFile"));
    minifi::utils::Identifier uuid = rootFlowConfig->findProcessorByName("GetFile")->getUUID();
    REQUIRE(uuid);
    REQUIRE(!rootFlowConfig->findProcessorByName("GetFile")->getUUIDStr().empty());
  } catch (const std::exception &e) {
    caught_exception = true;
    REQUIRE("Unable to parse configuration file for component named 'XYZ' because ValidationFailed" == std::string(e.what()));
  }

  REQUIRE(caught_exception);
}

TEST_CASE("Test Required Property 2", "[YamlConfigurationRequiredProperty2]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML = R"(
Flow Controller:
  name: Simple
Processors:
- name: XYZ
  class: GetFile
  Properties:
    Input Directory: "/"
    Batch Size: 1
      )";
  std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getRootFromPayload(TEST_CONFIG_YAML);

  REQUIRE(rootFlowConfig);
  REQUIRE(rootFlowConfig->findProcessorByName("XYZ"));
  minifi::utils::Identifier uuid = rootFlowConfig->findProcessorByName("XYZ")->getUUID();
  REQUIRE(uuid);
  REQUIRE(!rootFlowConfig->findProcessorByName("XYZ")->getUUIDStr().empty());
}

class DummyComponent : public core::ConfigurableComponentImpl, public core::CoreComponentImpl {
 public:
  DummyComponent() : ConfigurableComponentImpl(), CoreComponentImpl("DummyComponent") {}

  bool supportsDynamicProperties() const override {
    return false;
  }

  bool supportsDynamicRelationships() const override {
    return false;
  }

  bool canEdit() override {
    return true;
  }
};

TEST_CASE("Test Dependent Property", "[YamlConfigurationDependentProperty]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());
  const auto component = std::make_shared<DummyComponent>();
  component->setSupportedProperties(std::to_array<core::PropertyReference>({
    core::PropertyDefinitionBuilder<>::createProperty("Prop A").withDescription("Prop A desc").withDefaultValue("val A").isRequired(true).build(),
    core::PropertyDefinitionBuilder<0, 1>::createProperty("Prop B").withDescription("Prop B desc").withDefaultValue("val B").isRequired(true).withDependentProperties({ "Prop A" }).build()
  }));
  yamlConfig.validateComponentProperties(*component, "component A", "section A");
  REQUIRE(true);  // Expected to get here w/o any exceptions
}

TEST_CASE("Test Dependent Property 2", "[YamlConfigurationDependentProperty2]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());
  const auto component = std::make_shared<DummyComponent>();
  component->setSupportedProperties(std::to_array<core::PropertyReference>({
    core::PropertyDefinitionBuilder<>::createProperty("Prop A").withDescription("Prop A desc").isRequired(false).build(),
    core::PropertyDefinitionBuilder<0, 1>::createProperty("Prop B").withDescription("Prop B desc").withDefaultValue("val B").isRequired(true).withDependentProperties({ "Prop A" }).build()
  }));
  bool config_failed = false;
  try {
    yamlConfig.validateComponentProperties(*component, "component A", "section A");
  } catch (const std::exception &e) {
    config_failed = true;
    REQUIRE("Unable to parse configuration file for component named 'component A' because property "
        "'Prop B' depends on property 'Prop A' which is not set "
        "[in 'section A' section of configuration file]" == std::string(e.what()));
  }
  REQUIRE(config_failed);
}

TEST_CASE("Test Exclusive Property", "[YamlConfigurationExclusiveOfProperty]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());
  const auto component = std::make_shared<DummyComponent>();
  component->setSupportedProperties(std::to_array<core::PropertyReference>({
    core::PropertyDefinitionBuilder<>::createProperty("Prop A").withDescription("Prop A desc").withDefaultValue("val A").isRequired(true).build(),
    core::PropertyDefinitionBuilder<0, 0, 1>::createProperty("Prop B").withDescription("Prop B desc").withDefaultValue("val B").isRequired(true)
        .withExclusiveOfProperties({{ { "Prop A", "^abcd.*$" } }}).build()
  }));
  yamlConfig.validateComponentProperties(*component, "component A", "section A");
  REQUIRE(true);  // Expected to get here w/o any exceptions
}

TEST_CASE("Test Exclusive Property 2", "[YamlConfigurationExclusiveOfProperty2]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());
  const auto component = std::make_shared<DummyComponent>();
  component->setSupportedProperties(std::to_array<core::PropertyReference>({
    core::PropertyDefinitionBuilder<>::createProperty("Prop A").withDescription("Prop A desc").withDefaultValue("val A").isRequired(true).build(),
    core::PropertyDefinitionBuilder<0, 0, 1>::createProperty("Prop B").withDescription("Prop B desc").withDefaultValue("val B").isRequired(true)
        .withExclusiveOfProperties({{ { "Prop A", "^val.*$" } }}).build()
  }));
  bool config_failed = false;
  try {
    yamlConfig.validateComponentProperties(*component, "component A", "section A");
  } catch (const std::exception &e) {
    config_failed = true;
    REQUIRE("Unable to parse configuration file for component named 'component A' because "
        "property 'Prop B' must not be set when the value of property 'Prop A' matches '^val.*$' "
        "[in 'section A' section of configuration file]" == std::string(e.what()));
  }
  REQUIRE(config_failed);
}

TEST_CASE("Test YAML Config With Funnel", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());

  static const std::string CONFIG_YAML_WITH_FUNNEL =
    R"(
MiNiFi Config Version: 3
Flow Controller:
  name: root
  comment: ''
Processors:
- id: 0eac51eb-d76c-4ba6-9f0c-351795b2d243
  name: GenerateFlowFile1
  class: org.apache.nifi.minifi.processors.GenerateFlowFile
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 10000 ms
  Properties:
    Batch Size: '1'
    Custom Text: custom1
    Data Format: Binary
    File Size: 1 kB
    Unique FlowFiles: 'true'
- id: 5ec49d9b-673d-4c6f-9108-ab4acce0c1dc
  name: GenerateFlowFile2
  class: org.apache.nifi.minifi.processors.GenerateFlowFile
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 10000 ms
  Properties:
    Batch Size: '1'
    Custom Text: other2
    Data Format: Binary
    File Size: 1 kB
    Unique FlowFiles: 'true'
- id: 695658ba-5b6e-4c7d-9c95-5a980b622c1f
  name: LogAttribute
  class: org.apache.nifi.minifi.processors.LogAttribute
  max concurrent tasks: 1
  scheduling strategy: EVENT_DRIVEN
  auto-terminated relationships list:
  - success
  Properties:
    FlowFiles To Log: '0'
Funnels:
- id: 01a2f910-7050-41c1-8528-942764e7591d
Connections:
- id: 97c6bdfb-3909-499f-9ae5-011cbe8cadaf
  name: 01a2f910-7050-41c1-8528-942764e7591d//LogAttribute
  source id: 01a2f910-7050-41c1-8528-942764e7591d
  source relationship names: []
  destination id: 695658ba-5b6e-4c7d-9c95-5a980b622c1f
- id: 353e6bd5-5fca-494f-ae99-02572352c47a
  name: GenerateFlowFile2/success/01a2f910-7050-41c1-8528-942764e7591d
  source id: 5ec49d9b-673d-4c6f-9108-ab4acce0c1dc
  source relationship names:
  - success
  destination id: 01a2f910-7050-41c1-8528-942764e7591d
- id: 9c02c302-eb4f-4aac-98ed-0f6720a4ff1b
  name: GenerateFlowFile1/success/01a2f910-7050-41c1-8528-942764e7591d
  source id: 0eac51eb-d76c-4ba6-9f0c-351795b2d243
  source relationship names:
  - success
  destination id: 01a2f910-7050-41c1-8528-942764e7591d
Remote Process Groups: []
    )";

  std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getRootFromPayload(CONFIG_YAML_WITH_FUNNEL);

  REQUIRE(rootFlowConfig);
  REQUIRE(rootFlowConfig->findProcessorByName("GenerateFlowFile1"));
  REQUIRE(rootFlowConfig->findProcessorByName("GenerateFlowFile2"));
  REQUIRE(rootFlowConfig->findProcessorById(minifi::utils::Identifier::parse("01a2f910-7050-41c1-8528-942764e7591d").value()));

  std::map<std::string, minifi::Connection*> connectionMap;
  rootFlowConfig->getConnections(connectionMap);
  REQUIRE(6 == connectionMap.size());
  for (const auto& it : connectionMap) {
    REQUIRE(it.second);
    REQUIRE(!it.second->getUUIDStr().empty());
    REQUIRE(it.second->getDestination());
    REQUIRE(it.second->getSource());
  }
}

TEST_CASE("Test UUID duplication checks", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  for (char i = '1'; i <= '8'; ++i) {
    DYNAMIC_SECTION("Changing UUID 00000000-0000-0000-0000-00000000000" << i << " to be a duplicate") {
      std::string config_yaml =
        R"(
          Flow Controller:
            name: root
            comment: ''
          Processors:
          - id: 00000000-0000-0000-0000-000000000001
            name: GenerateFlowFile1
            class: org.apache.nifi.minifi.processors.GenerateFlowFile
          - id: 00000000-0000-0000-0000-000000000002
            name: LogAttribute
            class: org.apache.nifi.minifi.processors.LogAttribute
          Funnels:
          - id: 00000000-0000-0000-0000-000000000003
          - id: 99999999-9999-9999-9999-999999999999
          Connections:
          - id: 00000000-0000-0000-0000-000000000004
            name: 00000000-0000-0000-0000-000000000003//LogAttribute
            source id: 00000000-0000-0000-0000-000000000003
            source relationship names: []
            destination id: 00000000-0000-0000-0000-000000000002
          - id: 00000000-0000-0000-0000-000000000005
            name: GenerateFlowFile1/success/00000000-0000-0000-0000-000000000003
            source id: 00000000-0000-0000-0000-000000000001
            source relationship names:
            - success
            destination id: 00000000-0000-0000-0000-000000000003
          Remote Process Groups:
          - id: 00000000-0000-0000-0000-000000000006
            name: ''
            url: http://localhost:8080/nifi
            transport protocol: RAW
            Input Ports:
            - id: 00000000-0000-0000-0000-000000000007
              name: test2
              max concurrent tasks: 1
              use compression: false
            Output Ports: []
          Controller Services:
            - name: SSLContextService
              id: 00000000-0000-0000-0000-000000000008
              class: SSLContextService
            )";

      minifi::utils::string::replaceAll(config_yaml, std::string("00000000-0000-0000-0000-00000000000") + i, "99999999-9999-9999-9999-999999999999");
      REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(config_yaml), "General Operation: UUID 99999999-9999-9999-9999-999999999999 is duplicated in the flow configuration");
    }
  }
}

TEST_CASE("Configuration is not valid yaml", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());
  REQUIRE_THROWS(yaml_config.getRootFromPayload("}"));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Configuration is not valid yaml"));
}

namespace {
inline constexpr std::string_view TEST_FLOW_WITH_SENSITIVE_PROPERTIES = R"(MiNiFi Config Version: 3
Flow Controller:
  name: root
  comment: ''
Core Properties:
  flow controller graceful shutdown period: 10 sec
  flow service write delay interval: 500 ms
  administrative yield duration: 30 sec
  bored yield duration: 10 millis
  max concurrent threads: 1
  variable registry properties: ''
FlowFile Repository:
  implementation: org.apache.nifi.controller.repository.WriteAheadFlowFileRepository
  partitions: 256
  checkpoint interval: 2 mins
  always sync: false
  Swap:
    threshold: 20000
    in period: 5 sec
    in threads: 1
    out period: 5 sec
    out threads: 4
Content Repository:
  implementation: org.apache.nifi.controller.repository.FileSystemRepository
  content claim max appendable size: 10 MB
  content claim max flow files: 100
  content repository archive enabled: false
  content repository archive max retention period: 12 hours
  content repository archive max usage percentage: 50%
  always sync: false
Provenance Repository:
  provenance rollover time: 1 min
  implementation: org.apache.nifi.provenance.WriteAheadProvenanceRepository
  provenance index shard size: 500 MB
  provenance max storage size: 1 GB
  provenance max storage time: 24 hours
  provenance buffer size: 10000
Component Status Repository:
  buffer size: 1440
  snapshot frequency: 1 min
Security Properties:
  keystore: ''
  keystore type: ''
  keystore password: ''
  key password: ''
  truststore: ''
  truststore type: ''
  truststore password: ''
  ssl protocol: ''
  Sensitive Props:
    key:
    algorithm: NIFI_PBKDF2_AES_GCM_256
Processors:
- id: 5f69344a-15e6-43ba-911c-7e9dd2595d1c
  name: GenerateFlowFile
  class: org.apache.nifi.minifi.processors.GenerateFlowFile
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 10s
  penalization period: 30000 ms
  yield period: 1000 ms
  run duration nanos: 0
  auto-terminated relationships list: []
  Properties:
    Batch Size: '1'
    Data Format: Text
    File Size: 100 B
    Unique FlowFiles: 'true'
- id: f5f77e5d-4df2-4762-8ecc-d0dc07e36921
  name: InvokeHTTP
  class: org.apache.nifi.minifi.processors.InvokeHTTP
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1000 ms
  penalization period: 30000 ms
  yield period: 1000 ms
  run duration nanos: 0
  auto-terminated relationships list:
  - success
  - response
  - failure
  - retry
  - no retry
  Properties:
    Always Output Response: 'false'
    Connection Timeout: 5 s
    Content-type: application/octet-stream
    Disable Peer Verification: 'false'
    Follow Redirects: 'true'
    HTTP Method: POST
    Include Date Header: 'true'
    Invalid HTTP Header Field Handling Strategy: transform
    Penalize on "No Retry": 'false'
    Proxy Host: myproxy.com
    Proxy Port: '8080'
    Read Timeout: 15 s
    Remote URL: https://myremoteserver.com
    SSL Context Service: a00f8722-2419-44ee-929c-ad68644ad557
    Send Message Body: 'true'
    Use Chunked Encoding: 'false'
    invokehttp-proxy-password: password123
    invokehttp-proxy-username: user
    send-message-body: 'true'
Controller Services:
- id: a00f8722-2419-44ee-929c-ad68644ad557
  name: SSLContextService
  type: org.apache.nifi.minifi.controllers.SSLContextService
  Properties:
    CA Certificate:
    Client Certificate:
    Passphrase: secret1!!1!
    Private Key: /opt/secrets/private-key.pem
    Use System Cert Store: 'true'
Process Groups: []
Input Ports: []
Output Ports: []
Funnels: []
Connections:
- id: 361bf7a6-e63d-4967-95f5-7bbeec3175bc
  name: GenerateFlowFile/success/InvokeHTTP
  source id: 5f69344a-15e6-43ba-911c-7e9dd2595d1c
  source relationship names:
  - success
  destination id: f5f77e5d-4df2-4762-8ecc-d0dc07e36921
  max work queue size: 2000
  max work queue data size: 100 MB
  flowfile expiration: 0 seconds
  queue prioritizer class: ''
Remote Process Groups: []
NiFi Properties Overrides: {}
)";

inline constexpr std::string_view TEST_FLOW_WITH_SENSITIVE_PROPERTIES_ENCRYPTED = R"(MiNiFi Config Version: 3
Flow Controller:
  name: root
  comment: ""
Core Properties:
  flow controller graceful shutdown period: 10 sec
  flow service write delay interval: 500 ms
  administrative yield duration: 30 sec
  bored yield duration: 10 millis
  max concurrent threads: 1
  variable registry properties: ""
FlowFile Repository:
  implementation: org.apache.nifi.controller.repository.WriteAheadFlowFileRepository
  partitions: 256
  checkpoint interval: 2 mins
  always sync: false
  Swap:
    threshold: 20000
    in period: 5 sec
    in threads: 1
    out period: 5 sec
    out threads: 4
Content Repository:
  implementation: org.apache.nifi.controller.repository.FileSystemRepository
  content claim max appendable size: 10 MB
  content claim max flow files: 100
  content repository archive enabled: false
  content repository archive max retention period: 12 hours
  content repository archive max usage percentage: 50%
  always sync: false
Provenance Repository:
  provenance rollover time: 1 min
  implementation: org.apache.nifi.provenance.WriteAheadProvenanceRepository
  provenance index shard size: 500 MB
  provenance max storage size: 1 GB
  provenance max storage time: 24 hours
  provenance buffer size: 10000
Component Status Repository:
  buffer size: 1440
  snapshot frequency: 1 min
Security Properties:
  keystore: ""
  keystore type: ""
  keystore password: ""
  key password: ""
  truststore: ""
  truststore type: ""
  truststore password: ""
  ssl protocol: ""
  Sensitive Props:
    key: ~
    algorithm: NIFI_PBKDF2_AES_GCM_256
Processors:
  - id: 5f69344a-15e6-43ba-911c-7e9dd2595d1c
    name: GenerateFlowFile
    class: org.apache.nifi.minifi.processors.GenerateFlowFile
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 10s
    penalization period: 30000 ms
    yield period: 1000 ms
    run duration nanos: 0
    auto-terminated relationships list: []
    Properties:
      Batch Size: 1
      Data Format: Text
      File Size: 100 B
      Unique FlowFiles: true
  - id: f5f77e5d-4df2-4762-8ecc-d0dc07e36921
    name: InvokeHTTP
    class: org.apache.nifi.minifi.processors.InvokeHTTP
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 1000 ms
    penalization period: 30000 ms
    yield period: 1000 ms
    run duration nanos: 0
    auto-terminated relationships list:
      - success
      - response
      - failure
      - retry
      - no retry
    Properties:
      Always Output Response: false
      Connection Timeout: 5 s
      Content-type: application/octet-stream
      Disable Peer Verification: false
      Follow Redirects: true
      HTTP Method: POST
      Include Date Header: true
      Invalid HTTP Header Field Handling Strategy: transform
      Penalize on "No Retry": false
      Proxy Host: myproxy.com
      Proxy Port: 8080
      Read Timeout: 15 s
      Remote URL: https://myremoteserver.com
      SSL Context Service: a00f8722-2419-44ee-929c-ad68644ad557
      Send Message Body: true
      Use Chunked Encoding: false
      invokehttp-proxy-password: enc{...}
      invokehttp-proxy-username: user
      send-message-body: true
Controller Services:
  - id: a00f8722-2419-44ee-929c-ad68644ad557
    name: SSLContextService
    type: org.apache.nifi.minifi.controllers.SSLContextService
    Properties:
      CA Certificate: ~
      Client Certificate: ~
      Passphrase: enc{...}
      Private Key: /opt/secrets/private-key.pem
      Use System Cert Store: true
Process Groups: []
Input Ports: []
Output Ports: []
Funnels: []
Connections:
  - id: 361bf7a6-e63d-4967-95f5-7bbeec3175bc
    name: GenerateFlowFile/success/InvokeHTTP
    source id: 5f69344a-15e6-43ba-911c-7e9dd2595d1c
    source relationship names:
      - success
    destination id: f5f77e5d-4df2-4762-8ecc-d0dc07e36921
    max work queue size: 2000
    max work queue data size: 100 MB
    flowfile expiration: 0 seconds
    queue prioritizer class: ""
Remote Process Groups: []
NiFi Properties Overrides: {}
)";
}  // namespace

TEST_CASE("Test serialization", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());
  std::unique_ptr<core::ProcessGroup> root_flow_definition = yaml_config.getRootFromPayload(std::string{TEST_FLOW_WITH_SENSITIVE_PROPERTIES});
  REQUIRE(root_flow_definition);
  const std::string serialized_flow_definition = yaml_config.serialize(*root_flow_definition);
  // we can't directly compare the encrypted values, because they contain a random nonce
  const std::string serialized_flow_definition_masked = std::regex_replace(serialized_flow_definition, std::regex{"enc\\{.*\\}"}, "enc{...}");
  CHECK(serialized_flow_definition_masked == TEST_FLOW_WITH_SENSITIVE_PROPERTIES_ENCRYPTED);
}

TEST_CASE("Yaml configuration can use parameter contexts", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: Simple TailFile
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: lookup.frequency
      description: ''
      sensitive: false
      value: 12 min
    - name: batch_size
      description: ''
      sensitive: false
      value: 12
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: TailFile
  class: org.apache.nifi.processors.standard.TailFile
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Batch Size: "#{batch_size}"
    File to Tail: ./logs/minifi-app.log
    Initial Start Position: Beginning of File
    tail-mode: Single file
    Lookup frequency: "#{lookup.frequency}"
Controller Services: []
Process Groups: []
Input Ports: []
Output Ports: []
Funnels: []
Connections: []
Parameter Context Name: my-context
NiFi Properties Overrides: {}
      )";

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);
  auto* proc = flow->findProcessorByName("TailFile");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Batch Size") == "12");
  REQUIRE(proc->getProperty("Lookup frequency") == "12 min");
}

TEST_CASE("Yaml config should not replace parameter from different parameter context", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: Simple TailFile
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: lookup.frequency
      description: ''
      sensitive: false
      value: 12 min
  - id: 123e10b7-8e00-3188-9a27-476cca376978
    name: other-context
    description: my other context
    Parameters:
    - name: batch_size
      description: ''
      sensitive: false
      value: 1
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: TailFile
  class: org.apache.nifi.processors.standard.TailFile
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Batch Size: "#{batch_size}"
    File to Tail: ./logs/minifi-app.log
    Initial Start Position: Beginning of File
    tail-mode: Single file
    Lookup frequency: "#{lookup.frequency}"
Controller Services: []
Process Groups: []
Input Ports: []
Output Ports: []
Funnels: []
Connections: []
Parameter Context Name: my-context
NiFi Properties Overrides: {}
      )";

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), "Parameter Operation: Parameter 'batch_size' not found");
}

TEST_CASE("Cannot use the same parameter context name twice", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: Simple TailFile
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: lookup.frequency
      description: ''
      sensitive: false
      value: 12 min
  - id: 123e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: batch_size
      description: ''
      sensitive: false
      value: 1
Processors: []
Controller Services: []
Process Groups: []
Input Ports: []
Output Ports: []
Funnels: []
Connections: []
Parameter Context Name: my-context
NiFi Properties Overrides: {}
      )";

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), "Parameter context name 'my-context' already exists, parameter context names must be unique!");
}

TEST_CASE("Cannot use the same parameter name within a parameter context twice", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: Simple TailFile
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: lookup.frequency
      description: ''
      sensitive: false
      value: 12 min
    - name: lookup.frequency
      description: ''
      sensitive: false
      value: 1 min
Processors: []
Controller Services: []
Process Groups: []
Input Ports: []
Output Ports: []
Funnels: []
Connections: []
Parameter Context Name: my-context
NiFi Properties Overrides: {}
      )";

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML),
    "Parameter Operation: Parameter name 'lookup.frequency' already exists, parameter names must be unique within a parameter context!");
}

TEST_CASE("Cannot use non-sensitive parameter in sensitive property", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: my_value
      description: ''
      sensitive: false
      value: value1
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: TailFile
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: simple
    Sensitive Property: "#{my_value}"
Controller Services: []
Process Groups: []
Input Ports: []
Output Ports: []
Funnels: []
Connections: []
Parameter Context Name: my-context
NiFi Properties Overrides: {}
      )";

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), "Parameter Operation: Non-sensitive parameter 'my_value' cannot be referenced in a sensitive property");
}

TEST_CASE("Cannot use non-sensitive parameter in sensitive property value sequence", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: my_value
      description: ''
      sensitive: false
      value: value1
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: TailFile
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: simple
    Sensitive Property:
    - value: first value
    - value: "#{my_value}"
Controller Services: []
Process Groups: []
Input Ports: []
Output Ports: []
Funnels: []
Connections: []
Parameter Context Name: my-context
NiFi Properties Overrides: {}
      )";

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), "Parameter Operation: Non-sensitive parameter 'my_value' cannot be referenced in a sensitive property");
}

TEST_CASE("Parameters can be used in nested process groups", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: Simple TailFile
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: lookup.frequency
      description: ''
      sensitive: false
      value: 12 min
  - id: 123e10b7-8e00-3188-9a27-476cca376456
    name: sub-context
    description: my sub context
    Parameters:
    - name: batch_size
      description: ''
      sensitive: false
      value: 12
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: TailFile
  class: org.apache.nifi.processors.standard.TailFile
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Batch Size: 1
    File to Tail: ./logs/minifi-app.log
    Initial Start Position: Beginning of File
    tail-mode: Single file
    Lookup frequency: "#{lookup.frequency}"
Controller Services: []
Input Ports: []
Output Ports: []
Funnels: []
Connections: []
Parameter Context Name: my-context
Process Groups:
  - id: 2a3aaf32-8574-4fa7-b720-84001f8dde43
    name: Sub process group
    Processors:
    - id: 12304f28-0158-1000-0000-000000000000
      name: SubTailFile
      class: org.apache.nifi.processors.standard.TailFile
      max concurrent tasks: 1
      scheduling strategy: TIMER_DRIVEN
      scheduling period: 1 sec
      auto-terminated relationships list: [success]
      Properties:
        Batch Size: "#{batch_size}"
        File to Tail: ./logs/minifi-app.log
        Initial Start Position: Beginning of File
        tail-mode: Single file
        Lookup frequency: 1 sec
    Parameter Context Name: sub-context
      )";

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);
  auto* proc = flow->findProcessorByName("TailFile");
  REQUIRE(proc);
  CHECK(proc->getProperty("Batch Size") == "1");
  CHECK(proc->getProperty("Lookup frequency") == "12 min");
  auto* subproc = flow->findProcessorByName("SubTailFile");
  REQUIRE(subproc);
  CHECK(subproc->getProperty("Batch Size") == "12");
  CHECK(subproc->getProperty("Lookup frequency") == "1 sec");
}

TEST_CASE("Subprocessgroups cannot inherit parameters from parent processgroup", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: Simple TailFile
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: lookup.frequency
      description: ''
      sensitive: false
      value: 12 min
  - id: 123e10b7-8e00-3188-9a27-476cca376456
    name: sub-context
    description: my sub context
    Parameters:
    - name: batch_size
      description: ''
      sensitive: false
      value: 12
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: TailFile
  class: org.apache.nifi.processors.standard.TailFile
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Batch Size: 1
    File to Tail: ./logs/minifi-app.log
    Initial Start Position: Beginning of File
    tail-mode: Single file
    Lookup frequency: "#{lookup.frequency}"
Controller Services: []
Input Ports: []
Output Ports: []
Funnels: []
Connections: []
Parameter Context Name: my-context
Process Groups:
  - id: 2a3aaf32-8574-4fa7-b720-84001f8dde43
    name: Sub process group
    Processors:
    - id: 12304f28-0158-1000-0000-000000000000
      name: SubTailFile
      class: org.apache.nifi.processors.standard.TailFile
      max concurrent tasks: 1
      scheduling strategy: TIMER_DRIVEN
      scheduling period: 1 sec
      auto-terminated relationships list: [success]
      Properties:
        Batch Size: "#{batch_size}"
        File to Tail: ./logs/minifi-app.log
        Initial Start Position: Beginning of File
        tail-mode: Single file
        Lookup frequency: "#{lookup.frequency}"
    Parameter Context Name: sub-context
      )";

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), "Parameter Operation: Parameter 'lookup.frequency' not found");
}

TEST_CASE("Cannot use parameters if no parameter context is defined", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: TailFile
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: "#{my_value}"
      )";

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), "Parameter Operation: Property references a parameter in its value, but no parameter context was provided.");
}

TEST_CASE("Cannot use parameters in property value sequences if no parameter context is defined", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: TailFile
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property:
    - value: "#{first_value}"
    - value: "#{second_value}"
      )";

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), "Parameter Operation: Property references a parameter in its value, but no parameter context was provided.");
}

TEST_CASE("Property value sequences can use parameters", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flow
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: first_value
      description: ''
      sensitive: false
      value: value1
    - name: second_value
      description: ''
      sensitive: false
      value: value2
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property:
    - value: "#{first_value}"
    - value: "#{second_value}"
Parameter Context Name: my-context
      )";

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);
  auto* proc = flow->findProcessorByName("DummyProcessor");
  REQUIRE(proc);
  auto values = proc->getAllPropertyValues("Simple Property");
  REQUIRE(values);
  REQUIRE(values->size() == 2);
  CHECK((*values)[0] == "value1");
  CHECK((*values)[1] == "value2");
}

TEST_CASE("Dynamic properties can use parameters", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flow
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: first_value
      description: ''
      sensitive: false
      value: value1
    - name: second_value
      description: ''
      sensitive: false
      value: value2
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    My Dynamic Property Sequence:
    - value: "#{first_value}"
    - value: "#{second_value}"
    My Dynamic Property: "#{first_value}"
Parameter Context Name: my-context
      )";

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);

  core::Property property("My Dynamic Property Sequence", "");
  auto* proc = flow->findProcessorByName("DummyProcessor");
  REQUIRE(proc);
  auto values = proc->getAllDynamicPropertyValues("My Dynamic Property Sequence");
  REQUIRE(values);
  REQUIRE(values->size() == 2);
  CHECK((*values)[0] == "value1");
  CHECK((*values)[1] == "value2");

  REQUIRE(proc->getDynamicProperty("My Dynamic Property") == "value1");
}

TEST_CASE("Test sensitive parameters in sensitive properties", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_parameter_value = minifi::utils::crypto::property_encryption::encrypt("value1", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{my_value}", *context.sensitive_values_encryptor);
  core::YamlConfiguration yaml_config(context);

  static const std::string TEST_CONFIG_YAML =
      fmt::format(R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: my_value
      description: ''
      sensitive: true
      value: {encrypted_parameter_value}
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: simple
    Sensitive Property: {encrypted_sensitive_property_value}
Parameter Context Name: my-context
      )", fmt::arg("encrypted_parameter_value", encrypted_parameter_value), fmt::arg("encrypted_sensitive_property_value", encrypted_sensitive_property_value));

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);
  auto* proc = flow->findProcessorByName("DummyProcessor");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Simple Property") == "simple");
  REQUIRE(proc->getProperty("Sensitive Property") == "value1");
}

TEST_CASE("Test sensitive parameters in sensitive property value sequence", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_parameter_value_1 = minifi::utils::crypto::property_encryption::encrypt("value1", *context.sensitive_values_encryptor);
  auto encrypted_parameter_value_2 = minifi::utils::crypto::property_encryption::encrypt("value2", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value_1 = minifi::utils::crypto::property_encryption::encrypt("#{my_value_1}", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value_2 = minifi::utils::crypto::property_encryption::encrypt("#{my_value_2}", *context.sensitive_values_encryptor);
  core::YamlConfiguration yaml_config(context);

  static const std::string TEST_CONFIG_YAML =
      fmt::format(R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: my_value_1
      description: ''
      sensitive: true
      value: {encrypted_parameter_value_1}
    - name: my_value_2
      description: ''
      sensitive: true
      value: {encrypted_parameter_value_2}
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: simple
    Sensitive Property:
    - value: {encrypted_sensitive_property_value_1}
    - value: {encrypted_sensitive_property_value_2}
Parameter Context Name: my-context
      )", fmt::arg("encrypted_parameter_value_1", encrypted_parameter_value_1), fmt::arg("encrypted_parameter_value_2", encrypted_parameter_value_2),
          fmt::arg("encrypted_sensitive_property_value_1", encrypted_sensitive_property_value_1), fmt::arg("encrypted_sensitive_property_value_2", encrypted_sensitive_property_value_2));

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);
  auto* proc = flow->findProcessorByName("DummyProcessor");
  auto values = proc->getAllPropertyValues("Sensitive Property");
  REQUIRE(values);
  REQUIRE(values->size() == 2);
  CHECK((*values)[0] == "value1");
  CHECK((*values)[1] == "value2");
}

TEST_CASE("Test parameters in controller services", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_parameter_value_1 = minifi::utils::crypto::property_encryption::encrypt("secret1!!1!", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value_1 = minifi::utils::crypto::property_encryption::encrypt("#{my_value_1}", *context.sensitive_values_encryptor);
  core::YamlConfiguration yaml_config(context);

  static const std::string TEST_CONFIG_YAML =
      fmt::format(R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: my_value_1
      description: ''
      sensitive: true
      value: {encrypted_parameter_value_1}
    - name: my_value_2
      description: ''
      sensitive: false
      value: /opt/secrets/private-key.pem
Processors: []
Controller Services:
- id: a00f8722-2419-44ee-929c-ad68644ad557
  name: SSLContextService
  type: org.apache.nifi.minifi.controllers.SSLContextService
  Properties:
    CA Certificate:
    Client Certificate:
    Passphrase: {encrypted_sensitive_property_value_1}
    Private Key: "#{{my_value_2}}"
    Use System Cert Store: 'true'
Parameter Context Name: my-context
      )", fmt::arg("encrypted_parameter_value_1", encrypted_parameter_value_1), fmt::arg("encrypted_sensitive_property_value_1", encrypted_sensitive_property_value_1));

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);
  auto* controller = flow->findControllerService("SSLContextService");
  REQUIRE(controller);
  auto impl = controller->getControllerServiceImplementation();
  CHECK(impl->getProperty("Passphrase").value() == "secret1!!1!");
  CHECK(impl->getProperty("Private Key").value() == "/opt/secrets/private-key.pem");
}

TEST_CASE("Parameters can be used in controller services in nested process groups", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_parameter_value_1 = minifi::utils::crypto::property_encryption::encrypt("secret1!!1!", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value_1 = minifi::utils::crypto::property_encryption::encrypt("#{my_value_1}", *context.sensitive_values_encryptor);
  core::YamlConfiguration yaml_config(context);

  static const std::string TEST_CONFIG_YAML =
      fmt::format(R"(
MiNiFi Config Version: 3
Flow Controller:
  name: Simple TailFile
Parameter Contexts:
  - id: 123e10b7-8e00-3188-9a27-476cca376456
    name: sub-context
    description: my sub context
    Parameters:
    - name: my_value_1
      description: ''
      sensitive: true
      value: {encrypted_parameter_value_1}
    - name: my_value_2
      description: ''
      sensitive: false
      value: /opt/secrets/private-key.pem
Processors: []
Controller Services: []
Input Ports: []
Output Ports: []
Funnels: []
Connections: []
Process Groups:
  - id: 2a3aaf32-8574-4fa7-b720-84001f8dde43
    name: Sub process group
    Processors: []
    Controller Services:
    - id: a00f8722-2419-44ee-929c-ad68644ad557
      name: SSLContextService
      type: org.apache.nifi.minifi.controllers.SSLContextService
      Properties:
        CA Certificate:
        Client Certificate:
        Passphrase: {encrypted_sensitive_property_value_1}
        Private Key: "#{{my_value_2}}"
        Use System Cert Store: 'true'
    Parameter Context Name: sub-context
      )", fmt::arg("encrypted_parameter_value_1", encrypted_parameter_value_1), fmt::arg("encrypted_sensitive_property_value_1", encrypted_sensitive_property_value_1));

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);
  auto* controller = flow->findControllerService("SSLContextService", core::ProcessGroup::Traverse::IncludeChildren);
  REQUIRE(controller);
  auto impl = controller->getControllerServiceImplementation();
  REQUIRE(impl);
  CHECK(impl->getProperty("Passphrase").value() == "secret1!!1!");
  CHECK(impl->getProperty("Private Key").value() == "/opt/secrets/private-key.pem");
}

TEST_CASE("Test parameter context inheritance", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_parameter_value = minifi::utils::crypto::property_encryption::encrypt("value1", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{my_new_parameter}", *context.sensitive_values_encryptor);
  core::YamlConfiguration yaml_config(context);

  static const std::string TEST_CONFIG_YAML =
      fmt::format(R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: inherited-context
    description: my parameter context
    Parameters:
    - name: my_new_parameter
      description: inherited parameter context
      sensitive: true
      value: {}
    Inherited Parameter Contexts: [base-context]
  - id: 521e10b7-8e00-3188-9a27-476cca376351
    name: base-context
    description: my base parameter context
    Parameters:
    - name: my_old_parameter
      description: ''
      sensitive: false
      value: old_value
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: "#{{my_old_parameter}}"
    Sensitive Property: {}
Parameter Context Name: inherited-context
      )", encrypted_parameter_value, encrypted_sensitive_property_value);

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);
  auto* proc = flow->findProcessorByName("DummyProcessor");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Simple Property") == "old_value");
  REQUIRE(proc->getProperty("Sensitive Property") == "value1");
}

TEST_CASE("Parameter context can not inherit from a itself", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Contexts:
  - id: 521e10b7-8e00-3188-9a27-476cca376351
    name: base-context
    description: my base parameter context
    Parameters:
    - name: my_old_parameter
      description: ''
      sensitive: false
      value: old_value
    Inherited Parameter Contexts:
    - base-context
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: "#{my_old_parameter}"
Parameter Context Name: inherited-context
      )";

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), "Inherited parameter context 'base-context' cannot be the same as the parameter context!");
}

TEST_CASE("Parameter context can not inherit from non-existing parameter context", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Contexts:
  - id: 521e10b7-8e00-3188-9a27-476cca376351
    name: base-context
    description: my base parameter context
    Parameters:
    - name: my_old_parameter
      description: ''
      sensitive: false
      value: old_value
    Inherited Parameter Contexts: [unknown]
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: "#{my_old_parameter}"
Parameter Context Name: inherited-context
      )";

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), "Inherited parameter context 'unknown' does not exist!");
}

TEST_CASE("Cycles are not allowed in parameter context inheritance", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Contexts:
  - id: 123e10b7-8e00-3188-9a27-476cca376351
    name: a-context
    description: ''
    Parameters:
    - name: a_parameter
      description: ''
      sensitive: false
      value: a_value
    Inherited Parameter Contexts:
    - c-context
  - id: 456e10b7-8e00-3188-9a27-476cca376351
    name: b-context
    description: ''
    Parameters:
    - name: b_parameter
      description: ''
      sensitive: false
      value: b_value
    Inherited Parameter Contexts:
    - a-context
  - id: 789e10b7-8e00-3188-9a27-476cca376351
    name: c-context
    description: ''
    Parameters:
    - name: c_parameter
      description: ''
      sensitive: false
      value: c_value
    Inherited Parameter Contexts:
    - d-context
    - b-context
  - id: 101e10b7-8e00-3188-9a27-476cca376351
    name: d-context
    description: ''
    Parameters:
    - name: d_parameter
      description: ''
      sensitive: false
      value: d_value
    Inherited Parameter Contexts: []
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: "#{{my_old_parameter}}"
    Sensitive Property: {}
Parameter Context Name: inherited-context
      )";

  REQUIRE_THROWS_AS(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), std::invalid_argument);
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Circular references in Parameter Context inheritance are not allowed. Inheritance cycle was detected in parameter context"));
}

TEST_CASE("Parameter context inheritance order is respected", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: a-context
    description: ''
    Parameters:
    - name: a_parameter
      description: ''
      sensitive: false
      value: 1
    - name: b_parameter
      description: ''
      sensitive: false
      value: 2
  - id: 521e10b7-8e00-3188-9a27-476cca376351
    name: b-context
    description: ''
    Parameters:
    - name: b_parameter
      description: ''
      sensitive: false
      value: 3
    - name: c_parameter
      description: ''
      sensitive: false
      value: 4
  - id: 123e10b7-8e00-3188-9a27-476cca376351
    name: c-context
    description: ''
    Parameters:
    - name: c_parameter
      description: ''
      sensitive: false
      value: 5
    Inherited Parameter Contexts:
    - b-context
    - a-context
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    My A Property: "#{a_parameter}"
    My B Property: "#{b_parameter}"
    My C Property: "#{c_parameter}"
Parameter Context Name: c-context
      )";

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);
  auto* proc = flow->findProcessorByName("DummyProcessor");
  REQUIRE(proc);
  std::string value;
  REQUIRE("1" == proc->getDynamicProperty("My A Property"));
  REQUIRE("3" == proc->getDynamicProperty("My B Property"));
  REQUIRE("5" == proc->getDynamicProperty("My C Property"));
}

TEST_CASE("Parameter providers can be used for parameter values", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flow
Parameter Providers:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: DummyParameterProvider
    type: DummyParameterProvider
    Properties:
      Dummy1 Value: value1
      Dummy2 Value: value2
      Dummy3 Value: value3
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: "#{dummy1}"
    My Dynamic Property Sequence:
    - value: "#{dummy2}"
    - value: "#{dummy3}"
Parameter Context Name: dummycontext
      )";

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("DummyProcessor");
  REQUIRE(proc);
  auto values = proc->getAllDynamicPropertyValues("My Dynamic Property Sequence");
  CHECK((*values)[0] == "value2");
  CHECK((*values)[1] == "value3");
}

TEST_CASE("Parameter providers can be configured to select which parameters to be sensitive", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{dummy1}", *context.sensitive_values_encryptor);
  core::YamlConfiguration yaml_config(context);

  static const std::string TEST_CONFIG_YAML =
      fmt::format(R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Providers:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: DummyParameterProvider
    type: DummyParameterProvider
    Properties:
      Sensitive Parameter Scope: selected
      Sensitive Parameter List: dummy1
      Dummy1 Value: value1
      Dummy3 Value: value3
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: "#{{dummy3}}"
    Sensitive Property: {}
Parameter Context Name: dummycontext
      )", encrypted_sensitive_property_value);

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("DummyProcessor");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Sensitive Property") == "value1");
  REQUIRE(proc->getProperty("Simple Property") == "value3");
}

TEST_CASE("If sensitive parameter scope is set to selected sensitive parameter list is required", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{dummy1}", *context.sensitive_values_encryptor);
  core::YamlConfiguration yaml_config(context);

  static const std::string TEST_CONFIG_YAML =
      fmt::format(R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Providers:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: DummyParameterProvider
    type: DummyParameterProvider
    Properties:
      Sensitive Parameter Scope: selected
      Sensitive Parameter List:
      Dummy1 Value: value1
      Dummy3 Value: value3
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: "#{{dummy3}}"
    Sensitive Property: {}
Parameter Context Name: dummycontext
      )", encrypted_sensitive_property_value);

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), "Parameter Operation: Sensitive Parameter Scope is set to 'selected' but Sensitive Parameter List is empty");
}

TEST_CASE("Parameter providers can be configured to make all parameters sensitive", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{dummy1}", *context.sensitive_values_encryptor);
  core::YamlConfiguration yaml_config(context);

  static const std::string TEST_CONFIG_YAML =
      fmt::format(R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flowconfig
Parameter Providers:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: DummyParameterProvider
    type: DummyParameterProvider
    Properties:
      Sensitive Parameter Scope: all
      Dummy1 Value: value1
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Sensitive Property: {}
Parameter Context Name: dummycontext
      )", encrypted_sensitive_property_value);

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("DummyProcessor");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Sensitive Property") == "value1");
}

TEST_CASE("Parameter context can be inherited from parameter provider generated parameter context", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flow
Parameter Providers:
  - id: d26ee5f5-0192-1000-0482-4e333725e089
    name: DummyParameterProvider
    type: DummyParameterProvider
    Properties:
      Dummy1 Value: value1
      Dummy2 Value: value2
      Dummy3 Value: value3
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: my-context
    description: my parameter context
    Parameters:
    - name: file_size
      description: ''
      sensitive: false
      value: 10 B
    Inherited Parameter Contexts: [dummycontext]
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: "#{dummy1}"
Parameter Context Name: my-context
      )";

  std::unique_ptr<core::ProcessGroup> flow = yaml_config.getRootFromPayload(TEST_CONFIG_YAML);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("DummyProcessor");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Simple Property") == "value1");
}

TEST_CASE("Parameter context names cannot conflict with parameter provider generated parameter context names", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_config(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: flow
Parameter Providers:
  - id: d26ee5f5-0192-1000-0482-4e333725e089
    name: DummyParameterProvider
    type: DummyParameterProvider
    Properties:
      Dummy1 Value: value1
      Dummy2 Value: value2
      Dummy3 Value: value3
Parameter Contexts:
  - id: 721e10b7-8e00-3188-9a27-476cca376978
    name: dummycontext
    description: my parameter context
    Parameters:
    - name: file_size
      description: ''
      sensitive: false
      value: 10 B
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: DummyProcessor
  class: org.apache.nifi.processors.DummyProcessor
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1 sec
  auto-terminated relationships list: [success]
  Properties:
    Simple Property: "#{dummy1}"
Parameter Context Name: dummycontext
      )";

  REQUIRE_THROWS_WITH(yaml_config.getRootFromPayload(TEST_CONFIG_YAML), "Parameter provider 'DummyParameterProvider' cannot create parameter context 'dummycontext' because parameter context already "
    "exists with no parameter provider or generated by other parameter provider");
}

TEST_CASE("Cannot use invalid property values") {
  ConfigurationTestController test_controller;
  core::flow::AdaptiveConfiguration config(test_controller.getContext());
  auto [file_size_property_str, throws] = GENERATE(
    std::make_tuple(R"(12 dogBytes)", true),
    std::make_tuple(R"(1 kB)", false),
    std::make_tuple(R"(
    - value: 1 kB
    - value: 2 MB)", false),
    std::make_tuple(R"(foo)", true),
    std::make_tuple(R"(
    - value: 1 kB
    - value: bar KB)", true));
  const std::string CONFIG_YAML = fmt::format(
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: root
  comment: ''
Processors:
- id: 0eac51eb-d76c-4ba6-9f0c-351795b2d243
  name: GenerateFlowFile1
  class: org.apache.nifi.minifi.processors.GenerateFlowFile
  max concurrent tasks: 1
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 10000 ms
  Properties:
    File Size: {}
)", file_size_property_str);
  if (throws) {
    REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_YAML), "Unable to parse configuration file for component named 'GenerateFlowFile1' because ValidationFailed");
  } else {
    REQUIRE_NOTHROW(config.getRootFromPayload(CONFIG_YAML));
  }
}

TEST_CASE("Output port can also be used in RPG", "[YamlConfiguration]") {
  ConfigurationTestController test_controller;

  core::YamlConfiguration yamlConfig(test_controller.getContext());

  static const std::string TEST_CONFIG_YAML =
      R"(
MiNiFi Config Version: 3
Flow Controller:
  name: Simple RPG To PutFile
  comment: ''
Processors:
- id: b0c04f28-0158-1000-0000-000000000000
  name: PutFile
  class: org.apache.nifi.processors.standard.PutFile
  max concurrent tasks: 1
  scheduling strategy: EVENT_DRIVEN
  auto-terminated relationships list: []
  Properties: []

Connections:
- id: b0c0c3cc-0158-1000-0000-000000000000
  name: ac0e798c-0158-1000-0588-cda9b944e011/success/PutFile
  source id: ac0e798c-0158-1000-0588-cda9b944e011
  destination id: b0c04f28-0158-1000-0000-000000000000
Remote Process Groups:
- id: b0c09ff0-0158-1000-0000-000000000000
  name: ''
  url: http://localhost:8080/nifi
  comment: ''
  timeout: 20 sec
  yield period: 10 sec
  transport protocol: RAW
  Output Ports:
  - id: ac0e798c-0158-1000-0588-cda9b944e011
    name: AmazingOutputPort
    max concurrent tasks: 1
    use compression: true
    batch size:
      size: 10 B
      count: 3
      duration: 5 sec
  Input Ports: []
NiFi Properties Overrides: {}
      )";
  std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getRootFromPayload(TEST_CONFIG_YAML);

  TypedProcessorWrapper<minifi::RemoteProcessGroupPort> port =rootFlowConfig->findProcessorByName("AmazingOutputPort");
  REQUIRE(port);
  CHECK(port->getUUIDStr() == "ac0e798c-0158-1000-0588-cda9b944e011");
  CHECK(port.get().getUseCompression() == true);
  CHECK(port.get().getBatchSize().value() == 10);
  CHECK(port.get().getBatchCount().value() == 3);
  CHECK(port.get().getBatchDuration().value() == std::chrono::seconds(5));
  CHECK(port.get().getTimeout().value() == std::chrono::seconds(20));
}

}  // namespace org::apache::nifi::minifi::test
