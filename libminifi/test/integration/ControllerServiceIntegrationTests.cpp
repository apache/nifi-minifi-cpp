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
#define EXTENSION_LIST "*minifi-*"  // NOLINT(cppcoreguidelines-macro-usage)

#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <thread>
#include <vector>

#include "core/controller/ControllerServiceNodeMap.h"
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
#include "unit/TestUtils.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

REGISTER_RESOURCE(MockControllerService, ControllerService);
REGISTER_RESOURCE(MockProcessor, Processor);

void waitToVerifyProcessor() {
  std::this_thread::sleep_for(std::chrono::seconds(2));
}

TEST_CASE("ControllerServiceIntegrationTests", "[controller]") {
  using org::apache::nifi::minifi::test::utils::verifyEventHappenedInPollTime;
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();

  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestThreadedRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestControllerServices.yml";
  configuration->set(minifi::Configure::nifi_flow_configuration_file, test_file_path.string());
  std::string priv_key_file = "cn.ckey.pem";
  std::string passphrase = "cn.pass";
  configuration->set(minifi::Configure::nifi_security_client_certificate, test_file_path.string());
  configuration->set(minifi::Configure::nifi_security_client_private_key, priv_key_file);
  configuration->set(minifi::Configure::nifi_security_client_pass_phrase, passphrase);
  configuration->set(minifi::Configure::nifi_default_directory, TEST_RESOURCES);

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  content_repo->initialize(configuration);
  auto encryption_key = minifi::utils::string::from_hex("e4bce4be67f417ed2530038626da57da7725ff8c0b519b692e4311e4d4fe8a28");
  std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::make_unique<core::YamlConfiguration>(core::ConfigurationContext{
      .flow_file_repo = test_repo,
      .content_repo = content_repo,
      .configuration = configuration,
      .path = test_file_path,
      .filesystem = std::make_shared<minifi::utils::file::FileSystem>(),
      .sensitive_values_encryptor = minifi::utils::crypto::EncryptionProvider{minifi::utils::crypto::XSalsa20Cipher{encryption_key}}
  });
  const auto controller = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo);

  disabled = false;

  core::YamlConfiguration yaml_config(core::ConfigurationContext{
      .flow_file_repo = test_repo,
      .content_repo = content_repo,
      .configuration = configuration,
      .path = test_file_path,
      .filesystem = std::make_shared<minifi::utils::file::FileSystem>(),
      .sensitive_values_encryptor = minifi::utils::crypto::EncryptionProvider{minifi::utils::crypto::XSalsa20Cipher{encryption_key}}
  });
  auto pg = yaml_config.getRoot();

  auto provider = std::make_shared<core::controller::StandardControllerServiceProvider>(std::make_unique<core::controller::ControllerServiceNodeMap>(), std::make_shared<minifi::ConfigureImpl>());
  auto* mockNode = pg->findControllerService("MockItLikeIts1995");
  REQUIRE(mockNode != nullptr);
  mockNode->enable();
  std::vector<core::controller::ControllerServiceNode*> linkedNodes = mockNode->getLinkedControllerServices();
  REQUIRE(linkedNodes.size() == 1);

  core::controller::ControllerServiceNode* notexistNode = pg->findControllerService("MockItLikeItsWrong");
  REQUIRE(notexistNode == nullptr);

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_client;
  {
    std::lock_guard<std::mutex> lock(control_mutex);
    controller->load();
    controller->start();
    auto* ssl_client_node = controller->getControllerServiceNode("SSLClientServiceTest");
    REQUIRE(ssl_client_node != nullptr);
    ssl_client_node->enable();
    REQUIRE(ssl_client_node->getControllerServiceImplementation() != nullptr);
    ssl_client = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(ssl_client_node->getControllerServiceImplementation());
  }
  REQUIRE(!ssl_client->getCACertificate().empty());
  // now let's disable one of the controller services.
  const auto* const cs_id = controller->getControllerServiceNode("ID");
  REQUIRE(cs_id != nullptr);
  // TODO(adebreceni): MINIFICPP-1992
//  const auto checkCsIdEnabledMatchesDisabledFlag = [&cs_id] { return !disabled == cs_id->enabled(); };
//  {
//    std::lock_guard<std::mutex> lock(control_mutex);
//    controller->enableControllerService(cs_id);
//    disabled = false;
//  }
//  std::shared_ptr<core::controller::ControllerServiceNode> mock_cont = controller->getControllerServiceNode("MockItLikeIts1995");
//  REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(4), checkCsIdEnabledMatchesDisabledFlag));
//  {
//    std::lock_guard<std::mutex> lock(control_mutex);
//    controller->disableReferencingServices(mock_cont);
//    disabled = true;
//  }
//  REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(2), checkCsIdEnabledMatchesDisabledFlag));
//  {
//    std::lock_guard<std::mutex> lock(control_mutex);
//    controller->enableReferencingServices(mock_cont);
//    disabled = false;
//  }
//  REQUIRE(verifyEventHappenedInPollTime(std::chrono::seconds(2), checkCsIdEnabledMatchesDisabledFlag));

  controller->waitUnload(60s);
}

}  // namespace org::apache::nifi::minifi::test
