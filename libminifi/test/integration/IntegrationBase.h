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
#ifndef LIBMINIFI_TEST_INTEGRATION_INTEGRATIONBASE_H_
#define LIBMINIFI_TEST_INTEGRATION_INTEGRATIONBASE_H_

#include "../TestServer.h"
#include "../include/core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "CivetServer.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "controllers/SSLContextService.h"

int log_message(const struct mg_connection *conn, const char *message) {
  puts(message);
  return 1;
}

int ssl_enable(void *ssl_context, void *user_data) {
  struct ssl_ctx_st *ctx = (struct ssl_ctx_st *) ssl_context;
  return 0;
}

void waitToVerifyProcessor() {
  std::this_thread::sleep_for(std::chrono::seconds(3));
}

class IntegrationBase {
 public:
  IntegrationBase();

  void setUrl(std::string url, CivetHandler *handler);

  virtual ~IntegrationBase();

  void run(std::string test_file_location);

  void setKeyDir(const std::string key_dir) {
    this->key_dir = key_dir;
    configureSecurity();
  }

  virtual void testSetup() = 0;

  virtual void cleanup() = 0;

  virtual void runAssertions() = 0;

 protected:

  virtual void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) {

  }

  void configureSecurity();
  CivetServer *server;
  std::shared_ptr<minifi::Configure> configuration;
  std::string port, scheme, path;
  std::string key_dir;
};

IntegrationBase::IntegrationBase()
    : server(nullptr),
      configuration(std::make_shared<
          minifi::Configure>())
{
  mkdir("content_repository", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

IntegrationBase::~IntegrationBase()
{
  stop_webserver(server);
  rmdir("./content_repository");
}

void IntegrationBase::configureSecurity() {
  if (!key_dir.empty()) {
    configuration->set(minifi::Configure::nifi_security_client_certificate, key_dir + "cn.crt.pem");
    configuration->set(minifi::Configure::nifi_security_client_private_key, key_dir + "cn.ckey.pem");
    configuration->set(minifi::Configure::nifi_security_client_pass_phrase, key_dir + "cn.pass");
    configuration->set(minifi::Configure::nifi_security_client_ca_certificate, key_dir + "nifi-cert.pem");
    configuration->set(minifi::Configure::nifi_default_directory, key_dir);
  }
}

void IntegrationBase::run(std::string test_file_location) {
  testSetup();

  std::shared_ptr<core::Repository> test_repo =
      std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<
      TestFlowRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file,
                     test_file_location);

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(configuration);
  std::shared_ptr<minifi::io::StreamFactory> stream_factory = std::make_shared
      <minifi::io::StreamFactory>(configuration);
  std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr
      <core::YamlConfiguration
      >(new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory,
                                    configuration,
                                    test_file_location));

  core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory,
                                      configuration,
                                      test_file_location);

  std::unique_ptr<core::ProcessGroup> ptr = yaml_config.getRoot(
                                                                test_file_location);
  std::shared_ptr<core::ProcessGroup> pg = std::shared_ptr<core::ProcessGroup
      >(ptr.get());

  queryRootProcessGroup(pg);

  ptr.release();

  std::shared_ptr<TestRepository> repo = std::static_pointer_cast
      <TestRepository>(test_repo);

  std::shared_ptr<minifi::FlowController> controller =
      std::make_shared<minifi::FlowController
      >(test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo, DEFAULT_ROOT_GROUP_NAME, true);

  controller->load();
  controller->start();
  waitToVerifyProcessor();

  controller->waitUnload(60000);

  runAssertions();

  cleanup();
}

void IntegrationBase::setUrl(std::string url, CivetHandler *handler) {

  parse_http_components(url, port, scheme, path);
  struct mg_callbacks callback;
  if (url.find("localhost") != std::string::npos) {
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
  }
}

#endif /* LIBMINIFI_TEST_INTEGRATION_INTEGRATIONBASE_H_ */
