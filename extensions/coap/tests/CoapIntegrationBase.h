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
#ifndef EXTENSIONS_COAP_TESTS_COAPINTEGRATIONBASE_H
#define EXTENSIONS_COAP_TESTS_COAPINTEGRATIONBASE_H

#include "../tests/TestServer.h"
#include "CivetServer.h"
#include "integration/IntegrationBase.h"

int log_message(const struct mg_connection *conn, const char *message) {
  puts(message);
  return 1;
}

int ssl_enable(void *, void *) {
  return 0;
}

class CoapIntegrationBase : public IntegrationBase {
 public:
  CoapIntegrationBase(uint64_t waitTime = 5000)
      : IntegrationBase(waitTime),
        server(nullptr) {
  }

  void setUrl(std::string url, CivetHandler *handler);

  void shutdownBeforeFlowController() override {
    server.reset();
  }

  virtual void run(std::string test_file_location) override {
    testSetup();

    std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
    std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

    configuration->set(minifi::Configure::nifi_flow_configuration_file, test_file_location);
    configuration->set("c2.agent.heartbeat.period", "200");

    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(configuration);
    std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);
    std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<core::YamlConfiguration>(
        new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location));

    core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location);

    std::unique_ptr<core::ProcessGroup> ptr = yaml_config.getRoot(test_file_location);
    std::shared_ptr<core::ProcessGroup> pg = std::shared_ptr<core::ProcessGroup>(ptr.get());

    queryRootProcessGroup(pg);

    ptr.release();

    std::shared_ptr<TestRepository> repo = std::static_pointer_cast<TestRepository>(test_repo);

    std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo, DEFAULT_ROOT_GROUP_NAME,
                                                                                                  true);

    controller->load_without_reload();
    controller->start();

    runAssertions();

    shutdownBeforeFlowController();
    controller->waitUnload(wait_time_);
    controller->stopC2();

    cleanup();
  }

 protected:
  std::unique_ptr<TestServer> server;
};

void CoapIntegrationBase::setUrl(std::string url, CivetHandler *handler) {

  parse_http_components(url, port, scheme, path);
  CivetCallbacks callback{};
  if (url.find("localhost") != std::string::npos) {
    if (server != nullptr) {
      server->addHandler(path, handler);
      return;
    }
    if (scheme == "https" && !key_dir.empty()) {
      std::string cert = "";
      cert = key_dir + "nifi-cert.pem";
      memset(&callback, 0, sizeof(callback));
      callback.init_ssl = ssl_enable;
      port += "s";
      callback.log_message = log_message;
      server = utils::make_unique<TestServer>(port, path, handler, &callback, cert, cert);
    } else {
      server = utils::make_unique<TestServer>(port, path, handler);
    }
  }
}

#endif  // EXTENSIONS_COAP_TESTS_COAPINTEGRATIONBASE_H
