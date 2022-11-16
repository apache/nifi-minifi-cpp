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

#pragma once

#include <string>
#include <memory>
#include <utility>

#include "FlowController.h"
#include "unit/ProvenanceTestHelper.h"
#include "repository/VolatileContentRepository.h"
#include "CustomProcessors.h"

class TestControllerWithFlow: public TestController{
 public:
  explicit TestControllerWithFlow(const char* yamlConfigContent, bool setup_flow = true) {
    LogTestController::getInstance().setTrace<minifi::processors::TestProcessor>();
    LogTestController::getInstance().setTrace<minifi::processors::TestFlowFileGenerator>();
    LogTestController::getInstance().setTrace<minifi::Connection>();
    LogTestController::getInstance().setTrace<core::Connectable>();
    LogTestController::getInstance().setTrace<minifi::SchedulingAgent>();
    LogTestController::getInstance().setTrace<minifi::ThreadedSchedulingAgent>();
    LogTestController::getInstance().setTrace<core::Processor>();
    LogTestController::getInstance().setTrace<minifi::TimerDrivenSchedulingAgent>();
    LogTestController::getInstance().setTrace<minifi::EventDrivenSchedulingAgent>();
    LogTestController::getInstance().setTrace<minifi::FlowController>();

    home_ = createTempDirectory();

    yaml_path_ = home_ / "config.yml";
    std::ofstream{yaml_path_} << yamlConfigContent;

    configuration_ = std::make_shared<minifi::Configure>();
    configuration_->setHome(home_.string());
    configuration_->set(minifi::Configure::nifi_flow_configuration_file, yaml_path_.string());

    if (setup_flow) {
      setupFlow();
    }
  }

  void setupFlow() {
    std::shared_ptr<core::Repository> prov_repo = std::make_shared<TestThreadedRepository>();
    std::shared_ptr<core::Repository> ff_repo = std::make_shared<TestFlowRepository>();
    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

    REQUIRE(content_repo->initialize(configuration_));
    std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration_);

    auto flow = std::make_unique<core::YamlConfiguration>(prov_repo, ff_repo, content_repo, stream_factory, configuration_, yaml_path_.string());
    auto root = flow->getRoot();
    root_ = root.get();
    controller_ = std::make_shared<minifi::FlowController>(
        prov_repo, ff_repo, configuration_,
        std::move(flow),
        content_repo, DEFAULT_ROOT_GROUP_NAME,
        std::make_shared<utils::file::FileSystem>(), []{});
    controller_->load(std::move(root));
  }

  void startFlow() {
    controller_->start();
  }

  ~TestControllerWithFlow() {
    if (controller_) {
      controller_->stop();
      controller_->unload();
    }
    LogTestController::getInstance().reset();
  }

  std::filesystem::path home_;
  std::filesystem::path yaml_path_;
  std::shared_ptr<minifi::Configure> configuration_;
  std::shared_ptr<minifi::FlowController> controller_;
  core::ProcessGroup* root_{nullptr};
};
