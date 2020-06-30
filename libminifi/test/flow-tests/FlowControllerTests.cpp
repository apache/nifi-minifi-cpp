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
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "core/Core.h"
#include "core/repository/AtomicRepoEntries.h"
#include "core/RepositoryFactory.h"
#include "FlowFileRecord.h"
#include "provenance/Provenance.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "../TestBase.h"
#include "YamlConfiguration.h"

const char* yamlConfig =
R"(
Flow Controller:
    name: MiNiFi Flow
    id: 2438e3c8-015a-1000-79ca-83af40ec1990
Processors:
  - name: Generator
    id: 2438e3c8-015a-1000-79ca-83af40ec1991
    class: org.apache.nifi.processors.standard.GenerateFlowFile
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      Batch Size: 10
  - name: LogAttribute
    id: 2438e3c8-015a-1000-79ca-83af40ec1992
    class: org.apache.nifi.processors.standard.LogAttribute
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 1000 sec
    penalization period: 30 sec
    yield period: 1 sec
    run duration nanos: 0
    auto-terminated relationships list:

Connections:
  - name: Gen
    id: 2438e3c8-015a-1000-79ca-83af40ec1997
    source name: Generator
    source id: 2438e3c8-015a-1000-79ca-83af40ec1991
    source relationship name: success
    destination name: LogAttribute
    destination id: 2438e3c8-015a-1000-79ca-83af40ec1992
    max work queue size: 0
    max work queue data size: 1 MB
    flowfile expiration: 60 sec

Remote Processing Groups:

)";

TEST_CASE("Flow shutdown drains connections", "[TestFlow1]") {
  TestController testController;
  char format[] = "/tmp/flowTest.XXXXXX";
  std::string dir = testController.createTempDirectory(format);

  std::string yamlPath = utils::file::FileUtils::concat_path(dir, "config.yml");
  std::ofstream{yamlPath} << yamlConfig;

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<core::Repository> prov_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> ff_repo = std::make_shared<TestFlowRepository>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file, yamlPath);

  REQUIRE(content_repo->initialize(configuration));
  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);

  std::map<std::string, std::shared_ptr<minifi::Connection>> connectionMap;
  std::unique_ptr<core::FlowConfiguration> flow = utils::make_unique<core::YamlConfiguration>(prov_repo, ff_repo, content_repo, stream_factory, configuration, yamlPath);
  std::shared_ptr<core::ProcessGroup> root = flow->getRoot();
  std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(
      prov_repo, ff_repo, configuration,
      std::move(flow),
      content_repo, DEFAULT_ROOT_GROUP_NAME, true);


  root->getConnections(connectionMap);
  // adds the single connection to the map both by name and id
  REQUIRE(connectionMap.size() == 2);
  controller->load(root);
  controller->start();

  std::this_thread::sleep_for(std::chrono::milliseconds{1000});

  for (auto& it : connectionMap) {
    REQUIRE(it.second->getQueueSize() > 10);
  }

  controller->stop(true);

  for (auto& it : connectionMap) {
    REQUIRE(it.second->isEmpty());
  }
}
