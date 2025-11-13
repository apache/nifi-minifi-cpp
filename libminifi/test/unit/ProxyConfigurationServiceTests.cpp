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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "controllers/ProxyConfigurationService.h"

namespace org::apache::nifi::minifi::test {

struct ProxyConfigurationServiceTestFixture {
  ProxyConfigurationServiceTestFixture() {
    LogTestController::getInstance().clear();
    LogTestController::getInstance().setTrace<controllers::ProxyConfigurationService>();
  }

  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_ = test_controller_.createPlan();
  std::shared_ptr<core::controller::ControllerServiceNode>  proxy_configuration_node_ = plan_->addController("ProxyConfigurationService", "ProxyConfigurationService");
  std::shared_ptr<controllers::ProxyConfigurationService> proxy_configuration_service_ =
    std::dynamic_pointer_cast<controllers::ProxyConfigurationService>(proxy_configuration_node_->getControllerServiceImplementation());
};

TEST_CASE_METHOD(ProxyConfigurationServiceTestFixture, "ProxyConfigurationService onEnable throws when empty") {
  REQUIRE_THROWS_WITH(proxy_configuration_service_->onEnable(), "Process Schedule Operation: Proxy Server Host is required");
}

TEST_CASE_METHOD(ProxyConfigurationServiceTestFixture, "Only required properties are set in ProxyConfigurationService") {
  plan_->setProperty(proxy_configuration_node_, controllers::ProxyConfigurationService::ProxyServerHost, "192.168.1.123");
  REQUIRE_NOTHROW(plan_->finalize());
  auto proxy = proxy_configuration_service_->getProxyConfiguration();
  CHECK(proxy.proxy_host == "192.168.1.123");
  CHECK(proxy.proxy_port == std::nullopt);
  CHECK(proxy.proxy_user == std::nullopt);
  CHECK(proxy.proxy_password == std::nullopt);
}

TEST_CASE_METHOD(ProxyConfigurationServiceTestFixture, "All properties are set in ProxyConfigurationService") {
  plan_->setProperty(proxy_configuration_node_, controllers::ProxyConfigurationService::ProxyServerHost, "192.168.1.123");
  plan_->setProperty(proxy_configuration_node_, controllers::ProxyConfigurationService::ProxyServerPort, "8080");
  plan_->setProperty(proxy_configuration_node_, controllers::ProxyConfigurationService::ProxyUserName, "user");
  plan_->setProperty(proxy_configuration_node_, controllers::ProxyConfigurationService::ProxyUserPassword, "password");
  REQUIRE_NOTHROW(plan_->finalize());
  auto proxy = proxy_configuration_service_->getProxyConfiguration();
  CHECK(proxy.proxy_host == "192.168.1.123");
  CHECK(proxy.proxy_port == 8080);
  CHECK(proxy.proxy_user == "user");
  CHECK(proxy.proxy_password == "password");
}

}  // namespace org::apache::nifi::minifi::test
