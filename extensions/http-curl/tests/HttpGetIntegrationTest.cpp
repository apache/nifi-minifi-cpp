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
#include <sys/stat.h>
#undef NDEBUG
#include <cassert>
#include <utility>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include "HTTPClient.h"
#include "InvokeHTTP.h"
#include "TestServer.h"
#include "TestBase.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "processors/InvokeHTTP.h"
#include "processors/ListenHTTP.h"
#include "processors/LogAttribute.h"

void waitToVerifyProcessor() {
  std::this_thread::sleep_for(std::chrono::seconds(10));
}

int log_message(const struct mg_connection *conn, const char *message) {
  puts(message);
  return 1;
}

int ssl_enable(void *ssl_context, void *user_data) {
  struct ssl_ctx_st *ctx = (struct ssl_ctx_st *) ssl_context;
  return 0;
}

class HttpResponder : public CivetHandler {
 public:
  bool handleGet(CivetServer *server, struct mg_connection *conn) {
    static const std::string site2site_rest_resp = "hi this is a get test";
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
              "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              site2site_rest_resp.length());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }
};

int main(int argc, char **argv) {
  init_webserver();
  LogTestController::getInstance().setDebug<core::Processor>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<utils::HTTPClient>();
  LogTestController::getInstance().setDebug<minifi::controllers::SSLContextService>();
  LogTestController::getInstance().setDebug<minifi::processors::InvokeHTTP>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
  std::string key_dir, test_file_location;
  if (argc > 1) {
    test_file_location = argv[1];
    key_dir = argv[2];
  }
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  configuration->set(minifi::Configure::nifi_default_directory, key_dir);
  mkdir("content_repository", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file, test_file_location);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  content_repo->initialize(configuration);

  std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<core::YamlConfiguration>(
      new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location));
  std::shared_ptr<TestRepository> repo = std::static_pointer_cast<TestRepository>(test_repo);

  std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr),
                                                                                                content_repo,
                                                                                                DEFAULT_ROOT_GROUP_NAME,
                                                                                                true);

  core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location);

  std::unique_ptr<core::ProcessGroup> ptr = yaml_config.getRoot(test_file_location);
  std::shared_ptr<core::ProcessGroup> pg = std::shared_ptr<core::ProcessGroup>(ptr.get());
  std::shared_ptr<core::Processor> proc = ptr->findProcessor("invoke");
  assert(proc != nullptr);

  std::shared_ptr<minifi::processors::InvokeHTTP> inv = std::dynamic_pointer_cast<minifi::processors::InvokeHTTP>(proc);

  assert(inv != nullptr);
  std::string url = "";
  inv->getProperty(minifi::processors::InvokeHTTP::URL.getName(), url);
  ptr.release();
  HttpResponder h_ex;
  std::string port, scheme, path;
  CivetServer *server = nullptr;

  parse_http_components(url, port, scheme, path);
  struct mg_callbacks callback;
  if (url.find("localhost") != std::string::npos) {
    if (scheme == "https") {
      std::string cert = "";
      cert = key_dir + "nifi-cert.pem";
      memset(&callback, 0, sizeof(callback));
      callback.init_ssl = ssl_enable;
      port +="s";
      callback.log_message = log_message;
      server = start_webserver(port, path, &h_ex, &callback, cert, cert);
    } else {
      server = start_webserver(port, path, &h_ex);
    }
  }
  controller->load();
  controller->start();
  waitToVerifyProcessor();

  controller->waitUnload(60000);
  if (url.find("localhost") == std::string::npos) {
    stop_webserver(server);
    exit(1);
  }
  std::string logs = LogTestController::getInstance().log_output.str();

  assert(logs.find("key:filename value:") != std::string::npos);
  assert(logs.find("key:invokehttp.request.url value:" + url) != std::string::npos);
  assert(logs.find("key:invokehttp.status.code value:200") != std::string::npos);
  assert(logs.find("key:flow.id") != std::string::npos);

  LogTestController::getInstance().reset();
  rmdir("./content_repository");
  stop_webserver(server);
  return 0;
}
