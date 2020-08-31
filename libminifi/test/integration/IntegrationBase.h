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

#define DEFAULT_WAITTIME_MSECS 3000

#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "controllers/SSLContextService.h"
#include "HTTPUtils.h"

class IntegrationBase {
 public:
  IntegrationBase(uint64_t waitTime = DEFAULT_WAITTIME_MSECS);

  virtual ~IntegrationBase() = default;

  virtual void run(std::string test_file_location);

  void setKeyDir(const std::string key_dir) {
    this->key_dir = key_dir;
    configureSecurity();
  }

  virtual void testSetup() = 0;

  virtual void shutdownBeforeFlowController() {

  }

  virtual void cleanup() = 0;

  virtual void runAssertions() = 0;

 protected:

  virtual void configureC2() {
  }

  virtual void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) {

  }

  virtual void configureFullHeartbeat() {

  }

  virtual void updateProperties(std::shared_ptr<minifi::FlowController> fc) {

  }

  void configureSecurity();
  std::shared_ptr<minifi::Configure> configuration;
  std::shared_ptr<minifi::FlowController> flowController_;
  uint64_t wait_time_;
  std::string port, scheme, path;
  std::string key_dir;
  std::string state_dir;
};

IntegrationBase::IntegrationBase(uint64_t waitTime)
    : configuration(std::make_shared<minifi::Configure>()),
      wait_time_(waitTime) {
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

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file, test_file_location);

  configureC2();
  configureFullHeartbeat();

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(configuration);
  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);
  std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<core::YamlConfiguration>(
      new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location));

  core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location);

  auto controller_service_provider = yaml_ptr->getControllerServiceProvider();
  char state_dir_name_template[] = "/var/tmp/integrationstate.XXXXXX";
  state_dir = utils::file::FileUtils::create_temp_directory(state_dir_name_template);
  core::ProcessContext::getOrCreateDefaultStateManagerProvider(controller_service_provider.get(), configuration, state_dir.c_str());

  std::shared_ptr<core::ProcessGroup> pg(yaml_config.getRoot(test_file_location));
  queryRootProcessGroup(pg);

  std::shared_ptr<TestRepository> repo = std::static_pointer_cast<TestRepository>(test_repo);

  flowController_ = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo, DEFAULT_ROOT_GROUP_NAME,
                                                                                                true);
  flowController_->load_without_reload();
  updateProperties(flowController_);
  flowController_->start();

  runAssertions();

  shutdownBeforeFlowController();
  flowController_->unload();
  flowController_->stopC2();

  cleanup();
}

struct cmd_args {
  bool isUrlSecure() const {
    // check https prefix
    return url.rfind("https://", 0) == 0;
  }

  std::string test_file;
  std::string key_dir;
  std::string bad_test_file;
  std::string url;
};

cmd_args parse_basic_cmdline_args(int argc, char ** argv) {
  cmd_args args;
  if (argc > 1) {
    args.test_file = argv[1];
  }
  if (argc > 2) {
    args.key_dir = argv[2];
  }
  return args;
}

cmd_args parse_cmdline_args(int argc, char ** argv, const std::string& uri_path = "") {
  cmd_args args = parse_basic_cmdline_args(argc, argv);
  if (argc == 2) {
    args.url = "http://localhost:0/" + uri_path;
  }
  if (argc > 2) {
    args.url = "https://localhost:0/" + uri_path;
  }
  if (argc > 3) {
    args.bad_test_file = argv[3];
  }
  return args;
}

cmd_args parse_cmdline_args_with_url(int argc, char ** argv) {
  cmd_args args = parse_basic_cmdline_args(argc, argv);
  if (argc > 3) {
    std::string url = argv[3];
#ifdef WIN32
    if (url.find("localhost") != std::string::npos) {
      std::string port, scheme, path;
      parse_http_components(url, port, scheme, path);
      url = scheme + "://" + org::apache::nifi::minifi::io::Socket::getMyHostName() + ":" + port +  path;
    }
#endif
    args.url = url;
  }
  return args;
}

#endif /* LIBMINIFI_TEST_INTEGRATION_INTEGRATIONBASE_H_ */
