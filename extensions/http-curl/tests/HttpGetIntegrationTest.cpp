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
#include <thread>
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
#include "HTTPIntegrationBase.h"

namespace {
  void waitToVerifyProcessor() {
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
}

class HttpResponder : public CivetHandler {
 private:
 public:
  bool handleGet(CivetServer *server, struct mg_connection *conn) override {
    puts("handle get");
    static const std::string site2site_rest_resp = "hi this is a get test";
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
              "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              site2site_rest_resp.length());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv);

  LogTestController::getInstance().setDebug<core::Processor>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<utils::HTTPClient>();
  LogTestController::getInstance().setDebug<minifi::controllers::SSLContextService>();
  LogTestController::getInstance().setDebug<minifi::processors::InvokeHTTP>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  configuration->set(minifi::Configure::nifi_default_directory, args.key_dir);

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file, args.test_file);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  content_repo->initialize(configuration);

  std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<core::YamlConfiguration>(
      new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory, configuration, args.test_file));
  std::shared_ptr<TestRepository> repo = std::static_pointer_cast<TestRepository>(test_repo);

  std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr),
                                                                                                content_repo,
                                                                                                DEFAULT_ROOT_GROUP_NAME,
                                                                                                true);

  core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory, configuration, args.test_file);

  std::shared_ptr<core::Processor> proc = yaml_config.getRoot(args.test_file)->findProcessor("invoke");
  assert(proc != nullptr);

  const auto inv = std::dynamic_pointer_cast<minifi::processors::InvokeHTTP>(proc);
  assert(inv != nullptr);

  std::string url;
  inv->getProperty(minifi::processors::InvokeHTTP::URL.getName(), url);
  HttpResponder h_ex;
  std::string port, scheme, path;
  std::unique_ptr<TestServer> server;
  parse_http_components(url, port, scheme, path);
  CivetCallbacks callback{};
  if (scheme == "https") {
    std::string cert;
    cert = args.key_dir + "nifi-cert.pem";
    memset(&callback, 0, sizeof(callback));
    callback.init_ssl = ssl_enable;
    std::string https_port = port + "s";
    callback.log_message = log_message;
    server = utils::make_unique<TestServer>(https_port, path, &h_ex, &callback, cert, cert);
  } else {
    server = utils::make_unique<TestServer>(port, path, &h_ex);
  }
  controller->load();
  controller->start();
  waitToVerifyProcessor();

  controller->waitUnload(60000);
  if (url.find("localhost") == std::string::npos) {
    server.reset();
    exit(1);
  }
  std::string logs = LogTestController::getInstance().log_output.str();

  assert(logs.find("key:filename value:") != std::string::npos);
  assert(logs.find("key:invokehttp.request.url value:" + url) != std::string::npos);
  assert(logs.find("key:invokehttp.status.code value:200") != std::string::npos);
  assert(logs.find("key:flow.id") != std::string::npos);

  LogTestController::getInstance().reset();
  return 0;
}
