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
#include "TestBase.h"
#include "HTTPHandlers.h"
#include "HTTPClient.h"
#include "InvokeHTTP.h"
#include "processors/LogAttribute.h"
#include "core/state/ProcessorController.h"

#include "../tests/TestServer.h"
#include "CivetServer.h"
#include "HTTPIntegrationBase.h"

class VerifyInvokeHTTP : public CoapIntegrationBase {
public:
  VerifyInvokeHTTP()
      : CoapIntegrationBase(6000) {
  }

  virtual void testSetup() {
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setDebug<LogTestController>();
    LogTestController::getInstance().setTrace<minifi::processors::InvokeHTTP>();
    LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
  }

  virtual void cleanup() {
  }

  void setProperties(std::shared_ptr<core::Processor> proc) {
    std::string url = scheme + "://localhost:" + getWebPort() + path;
    proc->setProperty(minifi::processors::InvokeHTTP::URL.getName(), url);
  }

  void setupFlow(const std::string& flow_yml_path) {
    testSetup();

    std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
    std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

    configuration->set(minifi::Configure::nifi_flow_configuration_file, flow_yml_path);

    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(configuration);
    std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);
    std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<core::YamlConfiguration>(
        new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory, configuration, flow_yml_path));

    flowController_ = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo, DEFAULT_ROOT_GROUP_NAME, true);
    flowController_->load();

    const auto components = flowController_->getComponents("InvokeHTTP");
    assert(!components.empty());

    const auto stateController = components.at(0);
    assert(stateController);
    const auto processorController = std::dynamic_pointer_cast<minifi::state::ProcessorController>(stateController);
    assert(processorController);
    setProperties(processorController->getProcessor());
  }

  virtual void run(std::string flow_yml_path) override {
    setupFlow(flow_yml_path);
    startFlowController();

    waitToVerifyProcessor();
    shutdownBeforeFlowController();
    stopFlowController();
  }

  void startFlowController() {
    flowController_->start();
  }

  void stopFlowController() {
    flowController_->unload();
    flowController_->stopC2();

    runAssertions();
    cleanup();
  }
};

class VerifyInvokeHTTPOKResponse : public VerifyInvokeHTTP {
public:
  virtual void runAssertions() {
    assert(LogTestController::getInstance().contains("key:invokehttp.status.code value:201"));
    assert(LogTestController::getInstance().contains("response code 201"));
  }
};

class VerifyCouldNotConnectInvokeHTTP : public VerifyInvokeHTTP {
public:
  virtual void runAssertions() {
    assert(LogTestController::getInstance().contains("key:invoke_http value:failure"));
  }
};

class VerifyNoRetryInvokeHTTP : public VerifyInvokeHTTP {
public:
  virtual void runAssertions() {
    assert(LogTestController::getInstance().contains("key:invokehttp.status.message value:HTTP/1.1 404 Not Found"));
    assert(LogTestController::getInstance().contains("isSuccess: 0, response code 404"));
  }
};

class VerifyRetryInvokeHTTP : public VerifyInvokeHTTP {
public:
  virtual void runAssertions() {
    assert(LogTestController::getInstance().contains("key:invokehttp.status.message value:HTTP/1.1 501 Not Implemented"));
    assert(LogTestController::getInstance().contains("isSuccess: 0, response code 501"));
  }
};

class VerifyRWTimeoutInvokeHTTP : public VerifyInvokeHTTP {
public:
  virtual void runAssertions() {
    assert(LogTestController::getInstance().contains("key:invoke_http value:failure"));
    assert(LogTestController::getInstance().contains("failed Timeout was reached"));
  }
};

void run(VerifyInvokeHTTP& harness,
    const std::string& url,
    const std::string& test_file_location,
    const std::string& key_dir,
    CivetHandler * handler) {

  harness.setKeyDir(key_dir);
  harness.setUrl(url, handler);
  harness.run(test_file_location);
}

int main(int argc, char ** argv) {
  std::string key_dir, test_file_location, url;
  if (argc > 1) {
    test_file_location = argv[1];
    url = "http://localhost:0/minifi";
    if (argc > 2) {
      key_dir = argv[2];
      url = "https://localhost:0/minifi";
    }
  }

  // Stop civet server to simulate
  // unreachable remote end point
  {
    InvokeHTTPCouldNotConnectHandler handler;
    VerifyCouldNotConnectInvokeHTTP harness;
    harness.setKeyDir(key_dir);
    harness.setUrl(url, &handler);
    harness.setupFlow(test_file_location);
    harness.shutdownBeforeFlowController();
    harness.startFlowController();
    harness.waitToVerifyProcessor();
    harness.stopFlowController();
  }

  {
    InvokeHTTPResponseOKHandler handler;
    VerifyInvokeHTTPOKResponse harness;
    run(harness, url, test_file_location, key_dir, &handler);
  }

  {
    InvokeHTTPResponse404Handler handler;
    VerifyNoRetryInvokeHTTP harness;
    run(harness, url, test_file_location, key_dir, &handler);
  }

  {
    InvokeHTTPResponse501Handler handler;
    VerifyRetryInvokeHTTP harness;
    run(harness, url, test_file_location, key_dir, &handler);
  }

  {
    InvokeHTTPResponseTimeoutHandler handler(std::chrono::milliseconds(4000));
    VerifyRWTimeoutInvokeHTTP harness;
    run(harness, url, test_file_location, key_dir, &handler);
  }

  return 0;
}
