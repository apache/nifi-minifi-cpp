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
#include <cassert>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <thread>
#include <type_traits>
#include <vector>

#include "core/controller/ControllerServiceMap.h"
#include "core/controller/StandardControllerServiceNode.h"
#include "core/controller/StandardControllerServiceProvider.h"
#include "controllers/SSLContextService.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ProcessGroup.h"
#include "core/Resource.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/MockClasses.h"
#include "unit/ProvenanceTestHelper.h"

REGISTER_RESOURCE(MockControllerService, "");
REGISTER_RESOURCE(MockProcessor, "");

std::shared_ptr<core::controller::StandardControllerServiceNode> newCsNode(std::shared_ptr<core::controller::ControllerServiceProvider> provider, const std::string id) {
  std::shared_ptr<core::controller::ControllerService> service = std::make_shared<MockControllerService>();
  std::shared_ptr<core::controller::StandardControllerServiceNode> testNode = std::make_shared<core::controller::StandardControllerServiceNode>(service, provider, id,
                                                                                                                                                std::make_shared<minifi::Configure>());
  return testNode;
}

void waitToVerifyProcessor() {
  std::this_thread::sleep_for(std::chrono::seconds(2));
}

int main(int argc, char **argv) {
  std::string test_file_location;
  std::string key_dir;

  if (argc > 2) {
    test_file_location = argv[1];
    key_dir = argv[1];
  }

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file, test_file_location);
  std::string client_cert = "cn.crt.pem";
  std::string priv_key_file = "cn.ckey.pem";
  std::string passphrase = "cn.pass";
  std::string ca_cert = "nifi-cert.pem";
  configuration->set(minifi::Configure::nifi_security_client_certificate, test_file_location);
  configuration->set(minifi::Configure::nifi_security_client_private_key, priv_key_file);
  configuration->set(minifi::Configure::nifi_security_client_pass_phrase, passphrase);
  configuration->set(minifi::Configure::nifi_default_directory, key_dir);

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

  disabled = false;
  std::shared_ptr<core::controller::ControllerServiceMap> map = std::make_shared<core::controller::ControllerServiceMap>();

  core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location);

  std::unique_ptr<core::ProcessGroup> ptr = yaml_config.getRoot(test_file_location);
  std::shared_ptr<core::ProcessGroup> pg = std::shared_ptr<core::ProcessGroup>(ptr.get());
  ptr.release();

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
  assert(cs_id != nullptr);
  {
    std::lock_guard<std::mutex> lock(control_mutex);
    controller->disableControllerService(cs_id);
    disabled = true;
    waitToVerifyProcessor();
  }
  {
    std::lock_guard<std::mutex> lock(control_mutex);
    controller->enableControllerService(cs_id);
    disabled = false;
    waitToVerifyProcessor();
  }
  std::shared_ptr<core::controller::ControllerServiceNode> mock_cont = controller->getControllerServiceNode("MockItLikeIts1995");
  assert(cs_id->enabled());
{
    std::lock_guard<std::mutex> lock(control_mutex);
    controller->disableReferencingServices(mock_cont);
    disabled = true;
    waitToVerifyProcessor();
  }
    assert(cs_id->enabled() == false);
{
    std::lock_guard<std::mutex> lock(control_mutex);
    controller->enableReferencingServices(mock_cont);
    disabled = false;
    waitToVerifyProcessor();
  }
  assert(cs_id->enabled() == true);

  controller->waitUnload(60000);
  return 0;
}
