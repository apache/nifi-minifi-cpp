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

#include "../tests/TestServer.h"
#include "CivetServer.h"
#include "HTTPIntegrationBase.h"

class VerifyInvokeHTTP : public IntegrationBase {
public:
  VerifyInvokeHTTP()
      : IntegrationBase(6000),
        server(nullptr) {
  }

  virtual void testSetup() {
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setDebug<LogTestController>();
    LogTestController::getInstance().setTrace<minifi::processors::InvokeHTTP>();
    LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
  }

  virtual void shutdownBeforeFlowController() {
    stop_webserver(server);
  }

  virtual void cleanup() {
  }

  virtual void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) {
    std::shared_ptr<core::Processor> proc = pg->findProcessor("InvokeHTTP");
    assert(proc);
    std::shared_ptr<minifi::processors::InvokeHTTP> inv = std::dynamic_pointer_cast<minifi::processors::InvokeHTTP>(proc);
    assert(inv);
    std::string url = "";
    inv->getProperty(minifi::processors::InvokeHTTP::URL.getName(), url);
    assert(!url.empty());
    parse_http_components(url, port, scheme, path);
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

    core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory, configuration, flow_yml_path);

    std::unique_ptr<core::ProcessGroup> ptr = yaml_config.getRoot(flow_yml_path);
    std::shared_ptr<core::ProcessGroup> pg = std::shared_ptr<core::ProcessGroup>(ptr.get());

    queryRootProcessGroup(pg);

    configureFullHeartbeat();

    ptr.release();

    std::shared_ptr<TestRepository> repo = std::static_pointer_cast<TestRepository>(test_repo);

    flowController_ = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo, DEFAULT_ROOT_GROUP_NAME, true);
  }

  void startFlow() {
    updateProperties(flowController_);
    flowController_->load();
    flowController_->start();
  }

  void stopFlowAndVerify() {
    flowController_->unload();
    flowController_->stopC2();

    runAssertions();
    cleanup();
  }

  void startCivetServer(CivetHandler * handler) {
    if (server != nullptr) {
      server->addHandler(path, handler);
      return;
    }
    struct mg_callbacks callback;
    if (scheme == "https" && !key_dir.empty()) {
      std::string cert = "";
      cert = key_dir + "nifi-cert.pem";
      memset(&callback, 0, sizeof(callback));
      callback.init_ssl = ssl_enable;
      port += "s";
      callback.log_message = log_message;
      server = start_webserver(port, path, handler, &callback, cert, cert);
    } else {
      server = start_webserver(port, path, handler);
    }
    if (port == "0" || port == "0s") {
      bool secure = (port == "0s");
      port = std::to_string(server->getListeningPorts()[0]);
      if (secure) {
        port += "s";
      }
    }
  }

protected:
  CivetServer *server;
  bool isSecure;
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
    const std::string& test_file_location,
    const std::string& key_dir,
    CivetHandler * handler) {

  harness.setKeyDir(key_dir);
  harness.setupFlow(test_file_location);
  harness.startCivetServer(handler);
  harness.startFlow();
  harness.waitToVerifyProcessor();
  harness.shutdownBeforeFlowController();
  harness.stopFlowAndVerify();
}

int main(int argc, char ** argv) {
  std::string key_dir, test_file_location;
  if (argc > 1) {
    test_file_location = argv[1];
    if (argc > 2) {
      key_dir = argv[2];
    }
  }

  // Do not start the civet server to simulate
  // unreachable remote end point
  {
    VerifyCouldNotConnectInvokeHTTP harness;
    harness.setKeyDir(key_dir);
    harness.setupFlow(test_file_location);
    harness.startFlow();
    harness.waitToVerifyProcessor();
    harness.stopFlowAndVerify();
  }

  {
    InvokeHTTPResponseOKHandler handler;
    VerifyInvokeHTTPOKResponse harness;
    run(harness, test_file_location, key_dir, &handler);
  }

  {
    InvokeHTTPResponse404Handler handler;
    VerifyNoRetryInvokeHTTP harness;
    run(harness, test_file_location, key_dir, &handler);
  }

  {
    InvokeHTTPResponse501Handler handler;
    VerifyRetryInvokeHTTP harness;
    run(harness, test_file_location, key_dir, &handler);
  }

  {
    InvokeHTTPResponseTimeoutHandler handler(std::chrono::milliseconds(4000));
    VerifyRWTimeoutInvokeHTTP harness;
    run(harness, test_file_location, key_dir, &handler);
  }

  return 0;
}
