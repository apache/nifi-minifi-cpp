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

#define CURLOPT_SSL_VERIFYPEER_DISABLE 1
#undef NDEBUG
#include <cassert>
#include <utility>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include "HTTPClient.h"
#include "InvokeHTTP.h"
#include "TestServer.h"
#include "TestBase.h"
#include "utils/StringUtils.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "processors/LogAttribute.h"
#include "integration/IntegrationBase.h"
#include "utils/IntegrationTestUtils.h"

int log_message(const struct mg_connection* /*conn*/, const char *message) {
  puts(message);
  return 1;
}

int ssl_enable(void* /*ssl_context*/, void* /*user_data*/) {
  puts("Enable ssl");
  return 0;
}

class HttpResponder : public CivetHandler {
 private:
 public:
  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override {
    puts("handle get");
    static const std::string site2site_rest_resp = "hi this is a get test";
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
              "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              site2site_rest_resp.length());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }
};

class VerifyHTTPGetFixture {
 public:
  VerifyHTTPGetFixture(const cmd_args& args, HttpResponder& h_ex)
      : args_(args)
      , configuration_(std::make_shared<minifi::Configure>())
      , test_repo_(std::make_shared<TestRepository>())
      , test_flow_repo_(std::make_shared<TestFlowRepository>())
      , content_repo_(std::make_shared<core::repository::VolatileContentRepository>())
      , stream_factory_(minifi::io::StreamFactory::getInstance(configuration_)) {
    LogTestController::getInstance().setDebug<core::Processor>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setDebug<minifi::controllers::SSLContextService>();
    LogTestController::getInstance().setDebug<minifi::processors::InvokeHTTP>();
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

    setupFlow();
    setupURL();
    setupServer(h_ex);
    controller_->load();
  }

  ~VerifyHTTPGetFixture() {
    controller_->waitUnload(60000);
    LogTestController::getInstance().reset();
  }

  void run() {
    controller_->start();

    assert(org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime(
        std::chrono::seconds(10),
        "key:invokehttp.request.url value:" + url_,
        "key:invokehttp.status.code value:200",
        "key:flow.id"));
  }

 private:
  void setupFlow() {
    configuration_->set(minifi::Configure::nifi_default_directory, args_.key_dir);
    configuration_->set(minifi::Configure::nifi_flow_configuration_file, args_.test_file);
    content_repo_->initialize(configuration_);

    std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<core::YamlConfiguration>(
        new core::YamlConfiguration(test_repo_, test_repo_, content_repo_, stream_factory_, configuration_, args_.test_file));

    controller_ = std::make_shared<minifi::FlowController>(
        test_repo_, test_flow_repo_, configuration_, std::move(yaml_ptr), content_repo_, DEFAULT_ROOT_GROUP_NAME, true);
  }

  void setupURL() {
    core::YamlConfiguration yaml_config(test_repo_, test_repo_, content_repo_, stream_factory_, configuration_, args_.test_file);

    std::shared_ptr<core::Processor> proc = yaml_config.getRoot()->findProcessorByName("invoke");
    assert(proc != nullptr);

    const auto inv = std::dynamic_pointer_cast<minifi::processors::InvokeHTTP>(proc);
    assert(inv != nullptr);

    inv->getProperty(minifi::processors::InvokeHTTP::URL.getName(), url_);
  }

  void setupServer(HttpResponder& h_ex) {
    std::string port, scheme, path;
    parse_http_components(url_, port, scheme, path);
    CivetCallbacks callback{};
    if (scheme == "https") {
      std::string cert;
      cert = args_.key_dir + "nifi-cert.pem";
      callback.init_ssl = ssl_enable;
      std::string https_port = port + "s";
      callback.log_message = log_message;
      server_ = utils::make_unique<TestServer>(https_port, path, &h_ex, &callback, cert, cert);
    } else {
      server_ = utils::make_unique<TestServer>(port, path, &h_ex);
    }
  }

  cmd_args args_;
  std::shared_ptr<minifi::Configure> configuration_;
  std::shared_ptr<core::Repository> test_repo_;
  std::shared_ptr<core::Repository> test_flow_repo_;
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::shared_ptr<minifi::io::StreamFactory> stream_factory_;
  std::shared_ptr<minifi::FlowController> controller_;
  std::string url_;
  std::unique_ptr<TestServer> server_;
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv);

  HttpResponder h_ex;
  VerifyHTTPGetFixture test_fixture(args, h_ex);
  test_fixture.run();

  return 0;
}
