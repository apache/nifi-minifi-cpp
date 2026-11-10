// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "controllers/ProxyConfigurationService.h"
#include "unit/ControllerServiceUtils.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::test {

struct ProxyConfigurationServiceTestFixture {
  ProxyConfigurationServiceTestFixture() {
    LogTestController::getInstance().clear();
    LogTestController::getInstance().setTrace<controllers::ProxyConfigurationService>();
  }

  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_ = test_controller_.createPlan();
  std::shared_ptr<core::controller::ControllerServiceNode>  proxy_configuration_node_ = plan_->addController("ProxyConfigurationService", "ProxyConfigurationService");
  std::shared_ptr<controllers::ProxyConfigurationService> proxy_configuration_service_ = proxy_configuration_node_->getControllerServiceImplementation<controllers::ProxyConfigurationService>();
};

TEST_CASE_METHOD(ProxyConfigurationServiceTestFixture, "ProxyConfigurationService onEnable throws when invalid proxy type is set") {
  auto proxy_configuration_service = minifi::test::utils::make_controller_service<controllers::ProxyConfigurationService>(
    "ProxyConfigurationService", minifi::utils::IdGenerator::getIdGenerator()->generate());
  proxy_configuration_service->initialize();
  REQUIRE_THROWS_WITH(proxy_configuration_service->onEnable(), "Process Schedule Operation: Proxy Server Host is required");
}

TEST_CASE_METHOD(ProxyConfigurationServiceTestFixture, "Only required properties are set in ProxyConfigurationService") {
  plan_->setProperty(proxy_configuration_node_, controllers::ProxyConfigurationService::ProxyServerHost, "192.168.1.123");
  REQUIRE_NOTHROW(plan_->finalize());
  CHECK(proxy_configuration_service_->getHost() == "192.168.1.123");
  CHECK(proxy_configuration_service_->getPort() == 0);
  CHECK(proxy_configuration_service_->getProxyCredentials() == std::nullopt);
}

TEST_CASE_METHOD(ProxyConfigurationServiceTestFixture, "All properties are set in ProxyConfigurationService") {
  plan_->setProperty(proxy_configuration_node_, controllers::ProxyConfigurationService::ProxyServerHost, "192.168.1.123");
  plan_->setProperty(proxy_configuration_node_, controllers::ProxyConfigurationService::ProxyServerPort, "8080");
  plan_->setProperty(proxy_configuration_node_, controllers::ProxyConfigurationService::ProxyUserName, "user");
  plan_->setProperty(proxy_configuration_node_, controllers::ProxyConfigurationService::ProxyUserPassword, "password");
  REQUIRE_NOTHROW(plan_->finalize());
  CHECK(proxy_configuration_service_->getHost() == "192.168.1.123");
  CHECK(proxy_configuration_service_->getPort() == 8080);
  CHECK(proxy_configuration_service_->getProxyCredentials()->username == "user");
  CHECK(proxy_configuration_service_->getProxyCredentials()->password == "password");
}

}  // namespace org::apache::nifi::minifi::test
