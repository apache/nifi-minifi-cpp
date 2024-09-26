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

#include "core/PropertyDefinition.h"

#include <memory>
#include <utility>
#include <string>

#include "unit/TestBase.h"
#include "http/HTTPClient.h"
#include "InvokeHTTP.h"
#include "processors/LogAttribute.h"
#include "controllers/SSLContextService.h"
#include "core/state/ProcessorController.h"
#include "integration/HTTPIntegrationBase.h"
#include "unit/Catch.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/yaml/YamlConfiguration.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class VerifyInvokeHTTP : public HTTPIntegrationBase {
 public:
  VerifyInvokeHTTP()
      : HTTPIntegrationBase(6s) {
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::http::HTTPClient>();
    LogTestController::getInstance().setDebug<LogTestController>();
    LogTestController::getInstance().setTrace<minifi::processors::InvokeHTTP>();
    LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setDebug<core::Processor>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
    LogTestController::getInstance().setDebug<minifi::controllers::SSLContextService>();
  }

  void setUrl(const std::string &url, ServerAwareHandler *handler) override {
    if (path_) {
      throw std::logic_error("Url is already set");
    }
    std::string port, scheme, path;
    minifi::utils::parse_http_components(url, port, scheme, path);
    path_ = path;
    HTTPIntegrationBase::setUrl(url, handler);
  }

  void setProperties(const std::shared_ptr<core::Processor>& proc) {
    std::string url = scheme + "://localhost:" + getWebPort() + *path_;
    proc->setProperty(minifi::processors::InvokeHTTP::URL, url);
  }

  void setProperty(const core::PropertyReference& property, const std::string& value) {
    bool executed = false;
    flowController_->executeOnComponent("InvokeHTTP", [&](minifi::state::StateController& component) {
      const auto processorController = dynamic_cast<minifi::state::ProcessorController*>(&component);
      assert(processorController);
      auto& proc = processorController->getProcessor();
      proc.setProperty(property, std::string_view{value});
      executed = true;
    });

    REQUIRE(executed);
  }

  virtual void setupFlow(const std::optional<std::filesystem::path>& flow_yml_path) {
    testSetup();

    std::shared_ptr<core::Repository> test_repo = std::make_shared<TestThreadedRepository>();
    std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

    if (flow_yml_path) {
      configuration->set(minifi::Configure::nifi_flow_configuration_file, flow_yml_path->string());
    }
    configuration->set(minifi::Configure::nifi_c2_agent_heartbeat_period, "200");
    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(configuration);
    auto encryption_key = minifi::utils::string::from_hex("e4bce4be67f417ed2530038626da57da7725ff8c0b519b692e4311e4d4fe8a28");
    auto configuration_context = core::ConfigurationContext{
        .flow_file_repo = test_repo,
        .content_repo = content_repo,
        .configuration = configuration,
        .path = flow_yml_path,
        .filesystem = std::make_shared<minifi::utils::file::FileSystem>(),
        .sensitive_values_encryptor = minifi::utils::crypto::EncryptionProvider{minifi::utils::crypto::XSalsa20Cipher{encryption_key}}
    };
    auto yaml_ptr = std::make_unique<core::YamlConfiguration>(configuration_context);
    flowController_ = std::make_unique<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo);
    flowController_->load();

    std::string url = scheme + "://localhost:" + getWebPort() + *path_;
    setProperty(minifi::processors::InvokeHTTP::URL, url);
  }

  void run(const std::optional<std::filesystem::path>& flow_yml_path = {}, const std::optional<std::filesystem::path>& = {}) override {
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
    flowController_->stop();

    cleanup();
  }

 private:
  std::optional<std::string> path_;
};

}  // namespace org::apache::nifi::minifi::test
