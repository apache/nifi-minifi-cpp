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

#include <memory>
#include <string>
#include <core/RepositoryFactory.h>
#include "core/yaml/YamlConfiguration.h"
#include "../TestBase.h"

static const std::shared_ptr<core::Repository> TEST_PROV_REPO = core::createRepository("provenancerepository", true);
static const std::shared_ptr<core::Repository> TEST_FF_REPO = core::createRepository("flowfilerepository", true);

TEST_CASE("Test YAML Config 1", "[testyamlconfig1]") {

  static const std::string TEST_YAML_WITHOUT_IDS = "MiNiFi Config Version: 1\n"
      "Flow Controller:\n"
      "    name: MiNiFi Flow\n"
      "    comment:\n"
      "\n"
      "Core Properties:\n"
      "    flow controller graceful shutdown period: 10 sec\n"
      "    flow service write delay interval: 500 ms\n"
      "    administrative yield duration: 30 sec\n"
      "    bored yield duration: 10 millis\n"
      "\n"
      "FlowFile Repository:\n"
      "    partitions: 256\n"
      "    checkpoint interval: 2 mins\n"
      "    always sync: false\n"
      "    Swap:\n"
      "        threshold: 20000\n"
      "        in period: 5 sec\n"
      "        in threads: 1\n"
      "        out period: 5 sec\n"
      "        out threads: 4\n"
      "\n"
      "Provenance Repository:\n"
      "    provenance rollover time: 1 min\n"
      "\n"
      "Content Repository:\n"
      "    content claim max appendable size: 10 MB\n"
      "    content claim max flow files: 100\n"
      "    always sync: false\n"
      "\n"
      "Component Status Repository:\n"
      "    buffer size: 1440\n"
      "    snapshot frequency: 1 min\n"
      "\n"
      "Security Properties:\n"
      "    keystore: /tmp/ssl/localhost-ks.jks\n"
      "    keystore type: JKS\n"
      "    keystore password: localtest\n"
      "    key password: localtest\n"
      "    truststore: /tmp/ssl/localhost-ts.jks\n"
      "    truststore type: JKS\n"
      "    truststore password: localtest\n"
      "    ssl protocol: TLS\n"
      "    Sensitive Props:\n"
      "        key:\n"
      "        algorithm: PBEWITHMD5AND256BITAES-CBC-OPENSSL\n"
      "        provider: BC\n"
      "\n"
      "Processors:\n"
      "    - name: TailFile\n"
      "      class: org.apache.nifi.processors.standard.TailFile\n"
      "      max concurrent tasks: 1\n"
      "      scheduling strategy: TIMER_DRIVEN\n"
      "      scheduling period: 1 sec\n"
      "      penalization period: 30 sec\n"
      "      yield period: 1 sec\n"
      "      run duration nanos: 0\n"
      "      auto-terminated relationships list:\n"
      "      Properties:\n"
      "          File to Tail: logs/minifi-app.log\n"
      "          Rolling Filename Pattern: minifi-app*\n"
      "          Initial Start Position: Beginning of File\n"
      "\n"
      "Connections:\n"
      "    - name: TailToS2S\n"
      "      source name: TailFile\n"
      "      source relationship name: success\n"
      "      destination name: 8644cbcc-a45c-40e0-964d-5e536e2ada61\n"
      "      max work queue size: 0\n"
      "      max work queue data size: 1 MB\n"
      "      flowfile expiration: 60 sec\n"
      "      queue prioritizer class: org.apache.nifi.prioritizer.NewestFlowFileFirstPrioritizer\n"
      "\n"
      "Remote Processing Groups:\n"
      "    - name: NiFi Flow\n"
      "      comment:\n"
      "      url: https://localhost:8090/nifi\n"
      "      timeout: 30 secs\n"
      "      yield period: 10 sec\n"
      "      Input Ports:\n"
      "          - id: 8644cbcc-a45c-40e0-964d-5e536e2ada61\n"
      "            name: tailed log\n"
      "            comments:\n"
      "            max concurrent tasks: 1\n"
      "            use compression: false\n"
      "\n"
      "Provenance Reporting:\n"
      "    comment:\n"
      "    scheduling strategy: TIMER_DRIVEN\n"
      "    scheduling period: 30 sec\n"
      "    host: localhost\n"
      "    port name: provenance\n"
      "    port: 8090\n"
      "    port uuid: 2f389b8d-83f2-48d3-b465-048f28a1cb56\n"
      "    destination url: https://localhost:8090/\n"
      "    originating url: http://${hostname(true)}:8081/nifi\n"
      "    use compression: true\n"
      "    timeout: 30 secs\n"
      "    batch size: 1000";

  core::YamlConfiguration *yamlConfig = new core::YamlConfiguration(TEST_PROV_REPO, TEST_FF_REPO, std::make_shared<minifi::io::StreamFactory>(std::make_shared<minifi::Configure>()));
  std::istringstream yamlstream(TEST_YAML_WITHOUT_IDS);
  std::unique_ptr<core::ProcessGroup> rootFlowConfig = yamlConfig->getRoot(yamlstream);

  REQUIRE(rootFlowConfig);

  REQUIRE(rootFlowConfig->findProcessor("TailFile"));
  REQUIRE(NULL != rootFlowConfig->findProcessor("TailFile")->getUUID());
  REQUIRE(!rootFlowConfig->findProcessor("TailFile")->getUUIDStr().empty());
  REQUIRE(1 == rootFlowConfig->findProcessor("TailFile")->getMaxConcurrentTasks());
  REQUIRE(core::SchedulingStrategy::TIMER_DRIVEN == rootFlowConfig->findProcessor("TailFile")->getSchedulingStrategy());
  REQUIRE(1 == rootFlowConfig->findProcessor("TailFile")->getMaxConcurrentTasks());
  REQUIRE(1*1000*1000*1000 == rootFlowConfig->findProcessor("TailFile")->getSchedulingPeriodNano());
  REQUIRE(30*1000 == rootFlowConfig->findProcessor("TailFile")->getPenalizationPeriodMsec());
  REQUIRE(1*1000 == rootFlowConfig->findProcessor("TailFile")->getYieldPeriodMsec());
  REQUIRE(0 == rootFlowConfig->findProcessor("TailFile")->getRunDurationNano());

  std::map<std::string, std::shared_ptr<minifi::Connection>> connectionMap;
  rootFlowConfig->getConnections(connectionMap);
  REQUIRE(1 == connectionMap.size());
  // This is a map of UUID->Connection, and we don't know UUID, so just going to loop over it
  for(
      std::map<std::string,std::shared_ptr<minifi::Connection>>::iterator it = connectionMap.begin();
      it != connectionMap.end();
      ++it) {
    REQUIRE(it->second);
    REQUIRE(!it->second->getUUIDStr().empty());
    REQUIRE(it->second->getDestination());
    REQUIRE(it->second->getSource());
  }
}

TEST_CASE("Test YAML Config Missing Required Fields", "[testyamlconfig2]") {

  static const std::string TEST_YAML_NO_RPG_PORT_ID = "Flow Controller:\n"
      "  name: MiNiFi Flow\n"
      "Processors: []\n"
      "Connections: []\n"
      "Remote Processing Groups:\n"
      "    - name: NiFi Flow\n"
      "      comment:\n"
      "      url: https://localhost:8090/nifi\n"
      "      timeout: 30 secs\n"
      "      yield period: 10 sec\n"
      "      Input Ports:\n"
      "          - name: tailed log\n"
      "            comments:\n"
      "            max concurrent tasks: 1\n"
      "            use compression: false\n"
      "\n";

  core::YamlConfiguration *yamlConfig = new core::YamlConfiguration(TEST_PROV_REPO, TEST_FF_REPO, std::make_shared<minifi::io::StreamFactory>(std::make_shared<minifi::Configure>()));
  std::istringstream yamlstream(TEST_YAML_NO_RPG_PORT_ID);

  REQUIRE_THROWS_AS(yamlConfig->getRoot(yamlstream), std::invalid_argument);
}
