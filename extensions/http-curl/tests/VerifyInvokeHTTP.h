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

#undef NDEBUG

#include <memory>
#include <utility>
#include <string>

#include "TestBase.h"
#include "HTTPClient.h"
#include "InvokeHTTP.h"
#include "processors/LogAttribute.h"
#include "core/state/ProcessorController.h"
#include "HTTPIntegrationBase.h"

class VerifyInvokeHTTP : public HTTPIntegrationBase {
 public:
  VerifyInvokeHTTP()
      : HTTPIntegrationBase(6000) {
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setDebug<LogTestController>();
    LogTestController::getInstance().setTrace<minifi::processors::InvokeHTTP>();
    LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setDebug<core::Processor>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
  }

  void setUrl(const std::string &url, ServerAwareHandler *handler) override {
    if (path_) {
      throw std::logic_error("Url is already set");
    }
    std::string port, scheme, path;
    parse_http_components(url, port, scheme, path);
    path_ = path;
    HTTPIntegrationBase::setUrl(url, handler);
  }

  void setProperties(const std::shared_ptr<core::Processor>& proc) {
    std::string url = scheme + "://localhost:" + getWebPort() + *path_;
    proc->setProperty(minifi::processors::InvokeHTTP::URL.getName(), url);
  }

  void setProperty(const std::string& property, const std::string& value) {
    const auto components = flowController_->getComponents("InvokeHTTP");
    assert(!components.empty());

    const auto stateController = components[0];
    assert(stateController);
    const auto processorController = std::dynamic_pointer_cast<minifi::state::ProcessorController>(stateController);
    assert(processorController);
    auto proc = processorController->getProcessor();
    proc->setProperty(property, value);
  }

  virtual void setupFlow(const std::optional<std::string>& flow_yml_path) {
    testSetup();

    std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
    std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

    if (flow_yml_path) {
      configuration->set(minifi::Configure::nifi_flow_configuration_file, *flow_yml_path);
    }
    configuration->set("c2.agent.heartbeat.period", "200");
    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(configuration);
    std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);
    auto yaml_ptr = std::make_unique<core::YamlConfiguration>(test_repo, test_repo, content_repo, stream_factory, configuration, flow_yml_path);
    flowController_ = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo, DEFAULT_ROOT_GROUP_NAME);
    flowController_->load();

    std::string url = scheme + "://localhost:" + getWebPort() + *path_;
    setProperty(minifi::processors::InvokeHTTP::URL.getName(), url);
  }

  void run(const std::optional<std::string>& flow_yml_path = {}, const std::optional<std::string>& = {}) override {
    setupFlow(flow_yml_path);
    startFlowController();

    runAssertions();

    shutdownBeforeFlowController();
    stopFlowController();
  }

  void run(const std::string& url,
           const std::string& test_file_location,
           const std::string& key_dir,
           ServerAwareHandler* handler) {
    setKeyDir(key_dir);
    setUrl(url, handler);
    run(test_file_location);
  }

  void startFlowController() {
    flowController_->start();
  }

  void stopFlowController() {
    flowController_->unload();
    flowController_->stopC2();

    cleanup();
  }

 private:
  std::optional<std::string> path_;
};
