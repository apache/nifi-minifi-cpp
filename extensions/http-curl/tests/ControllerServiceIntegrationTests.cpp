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

#define EXTENSION_LIST "*minifi-*,!*jni*"

#undef NDEBUG
#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <thread>
#include <vector>

#include "core/controller/ControllerServiceMap.h"
#include "core/controller/StandardControllerServiceProvider.h"
#include "controllers/SSLContextService.h"
#include "core/ProcessGroup.h"
#include "core/Resource.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/MockClasses.h"
#include "unit/ProvenanceTestHelper.h"
#include "integration/IntegrationBase.h"
#include "utils/IntegrationTestUtils.h"

REGISTER_RESOURCE(MockControllerService, "");
REGISTER_RESOURCE(MockProcessor, "");

void waitToVerifyProcessor() {
  std::this_thread::sleep_for(std::chrono::seconds(2));
}

int main(int argc, char **argv) {
  using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
  const cmd_args args = parse_cmdline_args(argc, argv);

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file, args.test_file);
  std::string client_cert = "cn.crt.pem";
  std::string priv_key_file = "cn.ckey.pem";
  std::string passphrase = "cn.pass";
  std::string ca_cert = "nifi-cert.pem";
  configuration->set(minifi::Configure::nifi_security_client_certificate, args.test_file);
  configuration->set(minifi::Configure::nifi_security_client_private_key, priv_key_file);
  configuration->set(minifi::Configure::nifi_security_client_pass_phrase, passphrase);
  configuration->set(minifi::Configure::nifi_default_directory, args.key_dir);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(configuration);
  std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<core::YamlConfiguration>(
      new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory, configuration, args.test_file));
  std::shared_ptr<TestRepository> repo = std::static_pointer_cast<TestRepository>(test_repo);

  std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr),
                                                                                                content_repo,
                                                                                                DEFAULT_ROOT_GROUP_NAME);

  disabled = false;
  std::shared_ptr<core::controller::ControllerServiceMap> map = std::make_shared<core::controller::ControllerServiceMap>();

  core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory, configuration, args.test_file);

  std::shared_ptr<core::ProcessGroup> pg(yaml_config.getRoot());

  std::shared_ptr<core::controller::StandardControllerServiceProvider> provider = std::make_shared<core::controller::StandardControllerServiceProvider>(map, pg, std::make_shared<minifi::Configure>());
  std::shared_ptr<core::controller::ControllerServiceNode> mockNode = pg->findControllerService("MockItLikeIts1995");
  assert(mockNode != nullptr);
  mockNode->enable();
  std::vector<std::shared_ptr<core::controller::ControllerServiceNode> > linkedNodes = mockNode->getLinkedControllerServices();
  assert(linkedNodes.size() == 1);

  std::shared_ptr<core::controller::ControllerServiceNode> notexistNode = pg->findControllerService("MockItLikeItsWrong");
  assert(notexistNode == nullptr);

  std::shared_ptr<core::controller::ControllerServiceNode> ssl_client_cont = nullptr;
  std::shared_ptr<minifi::controllers::SSLContextService> ssl_client = nullptr;
  {
    std::lock_guard<std::mutex> lock(control_mutex);
    controller->load();
    controller->start();
    ssl_client_cont = controller->getControllerServiceNode("SSLClientServiceTest");
    ssl_client_cont->enable();
    assert(ssl_client_cont != nullptr);
    assert(ssl_client_cont->getControllerServiceImplementation() != nullptr);
    ssl_client = std::static_pointer_cast<minifi::controllers::SSLContextService>(ssl_client_cont->getControllerServiceImplementation());
  }
  assert(ssl_client->getCACertificate().length() > 0);
  // now let's disable one of the controller services.
  std::shared_ptr<core::controller::ControllerServiceNode> cs_id = controller->getControllerServiceNode("ID");
  const auto checkCsIdEnabledMatchesDisabledFlag = [&cs_id] { return !disabled == cs_id->enabled(); };
  assert(cs_id != nullptr);
  {
    std::lock_guard<std::mutex> lock(control_mutex);
    controller->enableControllerService(cs_id);
    disabled = false;
  }
  std::shared_ptr<core::controller::ControllerServiceNode> mock_cont = controller->getControllerServiceNode("MockItLikeIts1995");
  assert(verifyEventHappenedInPollTime(std::chrono::seconds(4), checkCsIdEnabledMatchesDisabledFlag));
  {
    std::lock_guard<std::mutex> lock(control_mutex);
    controller->disableReferencingServices(mock_cont);
    disabled = true;
  }
  assert(verifyEventHappenedInPollTime(std::chrono::seconds(2), checkCsIdEnabledMatchesDisabledFlag));
  {
    std::lock_guard<std::mutex> lock(control_mutex);
    controller->enableReferencingServices(mock_cont);
    disabled = false;
  }
  assert(verifyEventHappenedInPollTime(std::chrono::seconds(2), checkCsIdEnabledMatchesDisabledFlag));

  controller->waitUnload(60000);
  return 0;
}
