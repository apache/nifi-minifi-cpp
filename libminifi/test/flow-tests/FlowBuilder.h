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
#ifndef NIFI_MINIFI_CPP_FLOWCREATOR_H
#define NIFI_MINIFI_CPP_FLOWCREATOR_H

#include <unordered_map>
#include <string>
#include <random>
#include <YamlConfiguration.h>
#include "core/Processor.h"
#include "TestBase.h"

struct Flow{
  Flow(std::shared_ptr<minifi::FlowController>&& controller, std::shared_ptr<core::ProcessGroup>&& root)
  : controller_(std::move(controller)), root_(std::move(root)) {
    controller_->load(root_);
    controller_->start();
  }
  ~Flow() {
    controller_->stop(true);
    controller_->unload();
  }
  std::shared_ptr<minifi::FlowController> controller_;
  std::shared_ptr<core::ProcessGroup> root_;
};

Flow createFlow(const std::string& yamlPath) {
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<core::Repository> prov_repo = std::make_shared<core::repository::VolatileContentRepository>();
  std::shared_ptr<core::Repository> ff_repo = std::make_shared<core::repository::VolatileContentRepository>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file, yamlPath);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);
  content_repo->initialize(configuration);

  std::unique_ptr<core::FlowConfiguration> flow = utils::make_unique<core::YamlConfiguration>(prov_repo, ff_repo, content_repo, stream_factory, configuration, yamlPath);
  std::shared_ptr<core::ProcessGroup> root = flow->getRoot();

  std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(
      prov_repo, ff_repo, configuration,
      std::move(flow),
      content_repo, DEFAULT_ROOT_GROUP_NAME, true);

  return Flow{std::move(controller), std::move(root)};
}

#endif //NIFI_MINIFI_CPP_FLOWCREATOR_H
