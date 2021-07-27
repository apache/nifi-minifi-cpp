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
#include "core/repository/VolatileContentRepository.h"
#include "core/ProcessGroup.h"
#include "core/RepositoryFactory.h"
#include "core/yaml/YamlConfiguration.h"
#include "TailFile.h"
#include "TestBase.h"
#include "utils/TestUtils.h"

TEST_CASE("Test YAML Config Processing", "[YamlConfiguration]") {
  TestController test_controller;

  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);

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

    std::istringstream configYamlStream(CONFIG_YAML_WITHOUT_IDS);
    std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getYamlRoot(configYamlStream);

    REQUIRE(rootFlowConfig);
    REQUIRE(rootFlowConfig->findProcessorByName("TailFile"));
    utils::Identifier uuid = rootFlowConfig->findProcessorByName("TailFile")->getUUID();
    REQUIRE(uuid);
    REQUIRE(!rootFlowConfig->findProcessorByName("TailFile")->getUUIDStr().empty());
    REQUIRE(1 == rootFlowConfig->findProcessorByName("TailFile")->getMaxConcurrentTasks());
    REQUIRE(
        core::SchedulingStrategy::TIMER_DRIVEN == rootFlowConfig->findProcessorByName("TailFile")->getSchedulingStrategy());
    REQUIRE(1 == rootFlowConfig->findProcessorByName("TailFile")->getMaxConcurrentTasks());
    REQUIRE(1 * 1000 * 1000 * 1000 == rootFlowConfig->findProcessorByName("TailFile")->getSchedulingPeriodNano());
    REQUIRE(std::chrono::seconds(30) == rootFlowConfig->findProcessorByName("TailFile")->getPenalizationPeriod());
    REQUIRE(1 * 1000 == rootFlowConfig->findProcessorByName("TailFile")->getYieldPeriodMsec());
    REQUIRE(0 == rootFlowConfig->findProcessorByName("TailFile")->getRunDurationNano());

    std::map<std::string, std::shared_ptr<minifi::Connection>> connectionMap;
    rootFlowConfig->getConnections(connectionMap);
    REQUIRE(2 == connectionMap.size());
    // This is a map of UUID->Connection, and we don't know UUID, so just going to loop over it
    for (auto it : connectionMap) {
      REQUIRE(it.second);
      REQUIRE(!it.second->getUUIDStr().empty());
      REQUIRE(it.second->getDestination());
      REQUIRE(it.second->getSource());
      REQUIRE(60000 == it.second->getFlowExpirationDuration());
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

    std::istringstream configYamlStream(CONFIG_YAML_NO_RPG_PORT_ID);
    REQUIRE_THROWS_AS(yamlConfig.getYamlRoot(configYamlStream), std::invalid_argument);
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

    std::istringstream configYamlStream(CONFIG_YAML_EMPTY_RETRY_ATTRIBUTE);
    REQUIRE_THROWS_AS(yamlConfig.getYamlRoot(configYamlStream), utils::internal::InvalidValueException);
    REQUIRE(LogTestController::getInstance().contains("Invalid value was set for property 'Retry Attribute'"));
  }
}

TEST_CASE("Test YAML v3 Invalid Type", "[YamlConfiguration3]") {
  TestController test_controller;

  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);

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
  std::istringstream configYamlStream(TEST_CONFIG_YAML);

  REQUIRE_THROWS_AS(yamlConfig.getYamlRoot(configYamlStream), minifi::Exception);
}

TEST_CASE("Test YAML v3 Config Processing", "[YamlConfiguration3]") {
  TestController test_controller;

  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);

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
  transport protocol: RAW
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
  std::istringstream configYamlStream(TEST_CONFIG_YAML);
  std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getYamlRoot(configYamlStream);

  REQUIRE(rootFlowConfig);
  REQUIRE(rootFlowConfig->findProcessorByName("TailFile"));
  utils::Identifier uuid = rootFlowConfig->findProcessorByName("TailFile")->getUUID();
  REQUIRE(uuid);
  REQUIRE(!rootFlowConfig->findProcessorByName("TailFile")->getUUIDStr().empty());
  REQUIRE(1 == rootFlowConfig->findProcessorByName("TailFile")->getMaxConcurrentTasks());
  REQUIRE(core::SchedulingStrategy::TIMER_DRIVEN == rootFlowConfig->findProcessorByName("TailFile")->getSchedulingStrategy());
  REQUIRE(1 == rootFlowConfig->findProcessorByName("TailFile")->getMaxConcurrentTasks());
  REQUIRE(1 * 1000 * 1000 * 1000 == rootFlowConfig->findProcessorByName("TailFile")->getSchedulingPeriodNano());
  REQUIRE(std::chrono::seconds(30) == rootFlowConfig->findProcessorByName("TailFile")->getPenalizationPeriod());
  REQUIRE(1 * 1000 == rootFlowConfig->findProcessorByName("TailFile")->getYieldPeriodMsec());
  REQUIRE(0 == rootFlowConfig->findProcessorByName("TailFile")->getRunDurationNano());

  std::map<std::string, std::shared_ptr<minifi::Connection>> connectionMap;
  rootFlowConfig->getConnections(connectionMap);
  REQUIRE(2 == connectionMap.size());

  for (auto it : connectionMap) {
    REQUIRE(it.second);
    REQUIRE(!it.second->getUUIDStr().empty());
    REQUIRE(it.second->getDestination());
    REQUIRE(it.second->getSource());
    REQUIRE(0 == it.second->getFlowExpirationDuration());
  }
}

TEST_CASE("Test Dynamic Unsupported", "[YamlConfigurationDynamicUnsupported]") {
  TestController test_controller;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setTrace<core::YamlConfiguration>();

  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);

  static const std::string TEST_CONFIG_YAML = R"(
Flow Controller:
  name: Simple
Processors:
- name: PutFile
  class: PutFile
  Properties:
     Dynamic Property: Bad
      )";
  std::istringstream configYamlStream(TEST_CONFIG_YAML);
  std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getYamlRoot(configYamlStream);

  REQUIRE(rootFlowConfig);
  REQUIRE(rootFlowConfig->findProcessorByName("PutFile"));
  const utils::Identifier uuid = rootFlowConfig->findProcessorByName("PutFile")->getUUID();
  REQUIRE(uuid);
  REQUIRE(!rootFlowConfig->findProcessorByName("PutFile")->getUUIDStr().empty());

  REQUIRE(LogTestController::getInstance().contains("[warning] Unable to set the dynamic property "
                                                    "Dynamic Property with value Bad"));
}

TEST_CASE("Test Required Property", "[YamlConfigurationRequiredProperty]") {
  TestController test_controller;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<core::YamlConfiguration>();

  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);

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
  std::istringstream configYamlStream(TEST_CONFIG_YAML);
  bool caught_exception = false;

  try {
    std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getYamlRoot(configYamlStream);

    REQUIRE(rootFlowConfig);
    REQUIRE(rootFlowConfig->findProcessorByName("GetFile"));
    utils::Identifier uuid = rootFlowConfig->findProcessorByName("GetFile")->getUUID();
    REQUIRE(uuid);
    REQUIRE(!rootFlowConfig->findProcessorByName("GetFile")->getUUIDStr().empty());
  } catch (const std::exception &e) {
    caught_exception = true;
    REQUIRE("Unable to parse configuration file for component named 'XYZ' because required property "
        "'Input Directory' is not set [in 'Processors' section of configuration file]" == std::string(e.what()));
  }

  REQUIRE(caught_exception);
}

TEST_CASE("Test Required Property 2", "[YamlConfigurationRequiredProperty2]") {
  TestController test_controller;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<core::YamlConfiguration>();
  logTestController.setDebug<core::Processor>();

  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);

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
  std::istringstream configYamlStream(TEST_CONFIG_YAML);
  std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig.getYamlRoot(configYamlStream);

  REQUIRE(rootFlowConfig);
  REQUIRE(rootFlowConfig->findProcessorByName("XYZ"));
  utils::Identifier uuid = rootFlowConfig->findProcessorByName("XYZ")->getUUID();
  REQUIRE(uuid);
  REQUIRE(!rootFlowConfig->findProcessorByName("XYZ")->getUUIDStr().empty());
}

class DummyComponent : public core::ConfigurableComponent {
 public:
  virtual bool supportsDynamicProperties() {
    return false;
  }

  virtual bool canEdit() {
    return true;
  }
};

TEST_CASE("Test Dependent Property", "[YamlConfigurationDependentProperty]") {
  TestController test_controller;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<core::YamlConfiguration>();

  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);
  const auto component = std::make_shared<DummyComponent>();
  std::set<core::Property> props;
  props.emplace(core::Property("Prop A", "Prop A desc", "val A", true, "", { }, { }));
  props.emplace(core::Property("Prop B", "Prop B desc", "val B", true, "", { "Prop A" }, { }));
  component->setSupportedProperties(std::move(props));
  yamlConfig.validateComponentProperties(component, "component A", "section A");
  REQUIRE(true);  // Expected to get here w/o any exceptions
}

TEST_CASE("Test Dependent Property 2", "[YamlConfigurationDependentProperty2]") {
  TestController test_controller;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<core::YamlConfiguration>();

  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);
  const auto component = std::make_shared<DummyComponent>();
  std::set<core::Property> props;
  props.emplace(core::Property("Prop A", "Prop A desc", "", false, "", { }, { }));
  props.emplace(core::Property("Prop B", "Prop B desc", "val B", true, "", { "Prop A" }, { }));
  component->setSupportedProperties(std::move(props));
  bool config_failed = false;
  try {
    yamlConfig.validateComponentProperties(component, "component A", "section A");
  } catch (const std::exception &e) {
    config_failed = true;
    REQUIRE("Unable to parse configuration file for component named 'component A' because property "
        "'Prop B' depends on property 'Prop A' which is not set "
        "[in 'section A' section of configuration file]" == std::string(e.what()));
  }
  REQUIRE(config_failed);
}

#ifdef YAML_CONFIGURATION_USE_REGEX
TEST_CASE("Test Exclusive Property", "[YamlConfigurationExclusiveProperty]") {
  TestController test_controller;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<core::YamlConfiguration>();
  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);
  const auto component = std::make_shared<DummyComponent>();
  std::set<core::Property> props;
  props.emplace(core::Property("Prop A", "Prop A desc", "val A", true, "", { }, { }));
  props.emplace(core::Property("Prop B", "Prop B desc", "val B", true, "", { }, { { "Prop A", "^abcd.*$" } }));
  component->setSupportedProperties(std::move(props));
  yamlConfig.validateComponentProperties(component, "component A", "section A");
  REQUIRE(true);  // Expected to get here w/o any exceptions
}

TEST_CASE("Test Regex Property", "[YamlConfigurationRegexProperty]") {
  TestController test_controller;
  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<core::YamlConfiguration>();
  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);
  const auto component = std::make_shared<DummyComponent>();
  std::set<core::Property> props;
  props.emplace(core::Property("Prop A", "Prop A desc", "val A", true, "", { }, { }));
  props.emplace(core::Property("Prop B", "Prop B desc", "val B", true, "^val.*$", { }, { }));
  component->setSupportedProperties(std::move(props));
  yamlConfig.validateComponentProperties(component, "component A", "section A");
  REQUIRE(true);  // Expected to get here w/o any exceptions
}

TEST_CASE("Test Exclusive Property 2", "[YamlConfigurationExclusiveProperty2]") {
  TestController test_controller;

  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<core::YamlConfiguration>();
  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);
  const auto component = std::make_shared<DummyComponent>();
  std::set<core::Property> props;
  props.emplace(core::Property("Prop A", "Prop A desc", "val A", true, "", { }, { }));
  props.emplace(core::Property("Prop B", "Prop B desc", "val B", true, "", { }, { { "Prop A", "^val.*$" } }));
  component->setSupportedProperties(std::move(props));
  bool config_failed = false;
  try {
    yamlConfig.validateComponentProperties(component, "component A", "section A");
  } catch (const std::exception &e) {
    config_failed = true;
    REQUIRE("Unable to parse configuration file for component named 'component A' because "
        "property 'Prop B' must not be set when the value of property 'Prop A' matches '^val.*$' "
        "[in 'section A' section of configuration file]" == std::string(e.what()));
  }
  REQUIRE(config_failed);
}

TEST_CASE("Test Regex Property 2", "[YamlConfigurationRegexProperty2]") {
  TestController test_controller;
  LogTestController &logTestController = LogTestController::getInstance();
  logTestController.setDebug<TestPlan>();
  logTestController.setDebug<core::YamlConfiguration>();
  std::shared_ptr<core::Repository> testProvRepo = core::createRepository("provenancerepository", true);
  std::shared_ptr<core::Repository> testFlowFileRepo = core::createRepository("flowfilerepository", true);
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<minifi::io::StreamFactory> streamFactory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  core::YamlConfiguration yamlConfig(testProvRepo, testFlowFileRepo, content_repo, streamFactory, configuration);
  const auto component = std::make_shared<DummyComponent>();
  std::set<core::Property> props;
  props.emplace(core::Property("Prop A", "Prop A desc", "val A", true, "", { }, { }));
  props.emplace(core::Property("Prop B", "Prop B desc", "val B", true, "^notval.*$", { }, { }));
  component->setSupportedProperties(std::move(props));
  bool config_failed = false;
  try {
    yamlConfig.validateComponentProperties(component, "component A", "section A");
  } catch (const std::exception &e) {
    config_failed = true;
    REQUIRE("Unable to parse configuration file for component named 'component A' because "
        "property 'Prop B' does not match validation pattern '^notval.*$' "
        "[in 'section A' section of configuration file]" == std::string(e.what()));
  }
  REQUIRE(config_failed);
}

#endif  // YAML_CONFIGURATION_USE_REGEX
