// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <vector>
#include <memory>
#include <utility>
#include <string>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/Processor.h"
#include "../../controller/Controller.h"
#include "c2/ControllerSocketProtocol.h"
#include "controllers/UpdatePolicyControllerService.h"
#include "unit/ControllerServiceUtils.h"

TEST_CASE("TestEmptyPolicy", "[test1]") {
  auto controller = minifi::test::utils::make_controller_service<minifi::controllers::UpdatePolicyControllerService>("TestService");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  controller->initialize();
  controller->onEnable();
  REQUIRE(false == controller->getImplementation<minifi::controllers::UpdatePolicyControllerService>()->canUpdate("anyproperty"));
}

TEST_CASE("TestAllowAll", "[test1]") {
  auto controller = minifi::test::utils::make_controller_service<minifi::controllers::UpdatePolicyControllerService>("TestService");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  controller->initialize();
  REQUIRE(controller->setProperty(minifi::controllers::UpdatePolicyControllerService::AllowAllProperties.name, "true"));
  controller->onEnable();
  REQUIRE(true == controller->getImplementation<minifi::controllers::UpdatePolicyControllerService>()->canUpdate("anyproperty"));
}

TEST_CASE("TestAllowAllFails", "[test1]") {
  auto controller = minifi::test::utils::make_controller_service<minifi::controllers::UpdatePolicyControllerService>("TestService");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  controller->initialize();
  REQUIRE(controller->setProperty(minifi::controllers::UpdatePolicyControllerService::AllowAllProperties.name, "false"));
  controller->onEnable();
  REQUIRE(false == controller->getImplementation<minifi::controllers::UpdatePolicyControllerService>()->canUpdate("anyproperty"));
}

TEST_CASE("TestEnableProperty", "[test1]") {
  auto controller = minifi::test::utils::make_controller_service<minifi::controllers::UpdatePolicyControllerService>("TestService");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  controller->initialize();
  REQUIRE(controller->setProperty(minifi::controllers::UpdatePolicyControllerService::AllowAllProperties.name, "false"));
  REQUIRE(controller->setProperty(minifi::controllers::UpdatePolicyControllerService::AllowedProperties.name, "anyproperty"));
  controller->onEnable();
  REQUIRE(true == controller->getImplementation<minifi::controllers::UpdatePolicyControllerService>()->canUpdate("anyproperty"));
}

TEST_CASE("TestDisableProperty", "[test1]") {
  auto controller = minifi::test::utils::make_controller_service<minifi::controllers::UpdatePolicyControllerService>("TestService");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  controller->initialize();
  REQUIRE(controller->setProperty(minifi::controllers::UpdatePolicyControllerService::AllowAllProperties.name, "true"));
  REQUIRE(controller->setProperty(minifi::controllers::UpdatePolicyControllerService::DisallowedProperties.name, "anyproperty"));
  REQUIRE(controller->appendProperty(minifi::controllers::UpdatePolicyControllerService::DisallowedProperties.name, "anyproperty2"));
  controller->onEnable();
  REQUIRE(false == controller->getImplementation<minifi::controllers::UpdatePolicyControllerService>()->canUpdate("anyproperty"));
  REQUIRE(false == controller->getImplementation<minifi::controllers::UpdatePolicyControllerService>()->canUpdate("anyproperty2"));
  REQUIRE(true == controller->getImplementation<minifi::controllers::UpdatePolicyControllerService>()->canUpdate("anyproperty3"));
}
