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
#include "CustomProcessors.h"

const char* yamlConfig =
    R"(
Flow Controller:
    name: MiNiFi Flow
    id: 2438e3c8-015a-1000-79ca-83af40ec1990
Processors:
  - name: Generator
    id: 2438e3c8-015a-1000-79ca-83af40ec1991
    class: org.apache.nifi.processors.standard.TestFlowFileGenerator
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      Batch Size: 3
  - name: TestProcessor
    id: 2438e3c8-015a-1000-79ca-83af40ec1992
    class: org.apache.nifi.processors.standard.TestProcessor
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 3 sec
    yield period: 1 sec
    run duration nanos: 0
    auto-terminated relationships list:
      - apple
      - banana

Connections:
  - name: Gen
    id: 2438e3c8-015a-1000-79ca-83af40ec1997
    source name: Generator
    source id: 2438e3c8-015a-1000-79ca-83af40ec1991
    source relationship name: success
    destination name: TestProcessor
    destination id: 2438e3c8-015a-1000-79ca-83af40ec1992
    max work queue size: 0
    max work queue data size: 1 MB
    flowfile expiration: 60 sec

Remote Processing Groups:

)";

class TestControllerWithFlow: public TestController{
 public:
  TestControllerWithFlow() {
    char format[] = "/tmp/flowTest.XXXXXX";
    std::string dir = createTempDirectory(format);

    std::string yamlPath = utils::file::FileUtils::concat_path(dir, "config.yml");
    std::ofstream{yamlPath} << yamlConfig;

    configuration_ = std::make_shared<minifi::Configure>();
    std::shared_ptr<core::Repository> prov_repo = std::make_shared<TestRepository>();
    std::shared_ptr<core::Repository> ff_repo = std::make_shared<TestFlowRepository>();
    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

    configuration_->set(minifi::Configure::nifi_flow_configuration_file, yamlPath);

    REQUIRE(content_repo->initialize(configuration_));
    std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration_);

    std::unique_ptr<core::FlowConfiguration> flow = utils::make_unique<core::YamlConfiguration>(prov_repo, ff_repo, content_repo, stream_factory, configuration_, yamlPath);
    root_ = flow->getRoot();
    controller_ = std::make_shared<minifi::FlowController>(
        prov_repo, ff_repo, configuration_,
        std::move(flow),
        content_repo, DEFAULT_ROOT_GROUP_NAME, true);
  }
  std::shared_ptr<minifi::Configure> configuration_;
  std::shared_ptr<minifi::FlowController> controller_;
  std::shared_ptr<core::ProcessGroup> root_;
};

TEST_CASE("Flow shutdown waits for a while", "[TestFlow1]") {
  TestControllerWithFlow testController;
  auto controller = testController.controller_;
  auto root = testController.root_;

  auto sourceProc = std::static_pointer_cast<minifi::processors::TestFlowFileGenerator>(root->findProcessor("Generator"));
  auto sinkProc = std::static_pointer_cast<minifi::processors::TestProcessor>(root->findProcessor("TestProcessor"));

  controller->load(root);
  controller->start();

  std::this_thread::sleep_for(std::chrono::milliseconds{10});

  REQUIRE(sourceProc->trigger_count.load() == 1);
  REQUIRE(sinkProc->trigger_count.load() == 0);
  controller->stop(true);

  REQUIRE(sourceProc->trigger_count.load() == 1);
  REQUIRE(sinkProc->trigger_count.load() == 3);
}

TEST_CASE("Flow stopped after grace period", "[TestFlow2]") {
  TestControllerWithFlow testController;
  auto controller = testController.controller_;
  auto root = testController.root_;

  testController.configuration_->set(minifi::Configure::nifi_flowcontroller_drain_timeout, "1 s");

  auto sourceProc = std::static_pointer_cast<minifi::processors::TestFlowFileGenerator>(root->findProcessor("Generator"));
  auto sinkProc = std::static_pointer_cast<minifi::processors::TestProcessor>(root->findProcessor("TestProcessor"));

  sinkProc->onTriggerCb_ = []{
    std::this_thread::sleep_for(std::chrono::milliseconds{2000});
  };

  controller->load(root);
  controller->start();

  std::this_thread::sleep_for(std::chrono::milliseconds{10});

  REQUIRE(sourceProc->trigger_count.load() == 1);
  REQUIRE(sinkProc->trigger_count.load() == 0);
  controller->stop(true);


  REQUIRE(sourceProc->trigger_count.load() == 1);
  REQUIRE(sinkProc->trigger_count.load() == 1);
}
