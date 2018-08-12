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
#include <uuid/uuid.h>
#include <vector>
#include <memory>
#include <utility>
#include <string>
#include "../TestBase.h"
#include "io/ClientSocket.h"
#include "core/Processor.h"
#include "../../controller/Controller.h"
#include "core/controller/ControllerService.h"
#include "c2/ControllerSocketProtocol.h"
#include "controllers/NetworkPrioritizerService.h"
#include "state/UpdateController.h"

TEST_CASE("TestPrioritizerOneInterface", "[test1]") {
  auto controller = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  controller->initialize();
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth0,eth1");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "10 B");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::MaxPayload, "10 B");
  controller->onEnable();
  REQUIRE("eth0" == controller->getInterface(0).getInterface());
}

TEST_CASE("TestPrioritizerOneInterfaceMaxPayload", "[test2]") {
  auto controller = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  controller->initialize();
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth0,eth1");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "1 B");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::MaxPayload, "1 B");
  controller->onEnable();
  // can't because we've triggered the max payload
  REQUIRE("" == controller->getInterface(5).getInterface());
}

TEST_CASE("TestPrioritizerOneInterfaceMaxThroughput", "[test3]") {
  auto controller = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  controller->initialize();
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth0,eth1");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "10 B");
  controller->onEnable();
  // can't because we've triggered the max payload
  REQUIRE("eth0" == controller->getInterface(5).getInterface());
  REQUIRE("eth0" == controller->getInterface(5).getInterface());
  REQUIRE("" == controller->getInterface(5).getInterface());
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  REQUIRE("eth0" == controller->getInterface(5).getInterface());
}

TEST_CASE("TestPriorotizerMultipleInterfaces", "[test4]") {
  LogTestController::getInstance().setTrace<minifi::controllers::NetworkPrioritizerService>();

  auto controller = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService");
  auto controller2 = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService2");
  auto controller3 = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService3");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  controller->initialize();
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");

  controller3->initialize();
  controller3->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth0");
  controller3->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller3->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "10 B");
  controller3->onEnable();

  controller2->initialize();
  controller2->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth1");
  controller2->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller2->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "10 B");
  controller2->onEnable();
  std::vector<std::shared_ptr<core::controller::ControllerService> > services;
  services.push_back(controller2);
  services.push_back(controller3);
  controller->setLinkedControllerServices(services);
  controller->onEnable();
  // can't because we've triggered the max payload
  REQUIRE("eth1" == controller->getInterface(5).getInterface());
  REQUIRE("eth1" == controller->getInterface(5).getInterface());
  REQUIRE("eth0" == controller->getInterface(5).getInterface());
  REQUIRE("eth0" == controller->getInterface(5).getInterface());
}

TEST_CASE("TestPriorotizerMultipleInterfacesNeverSwitch", "[test5]") {
  auto controller = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService");
  auto controller2 = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService2");
  auto controller3 = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService3");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  controller->initialize();
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");

  controller3->initialize();
  controller3->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth0");
  controller3->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller3->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "1 kB");
  controller3->onEnable();

  controller2->initialize();
  controller2->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth1");
  controller2->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller2->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "10 B");
  controller2->onEnable();
  std::vector<std::shared_ptr<core::controller::ControllerService> > services;
  services.push_back(controller3);
  services.push_back(controller2);
  controller->setLinkedControllerServices(services);
  controller->onEnable();
  // can't because we've triggered the max payload
  for (int i = 0; i < 50; i++) {
    REQUIRE("eth0" == controller->getInterface(5).getInterface());
    REQUIRE("eth0" == controller->getInterface(5).getInterface());
    REQUIRE("eth0" == controller->getInterface(5).getInterface());
    REQUIRE("eth0" == controller->getInterface(5).getInterface());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}


TEST_CASE("TestPriorotizerMultipleInterfacesMaxPayload", "[test4]") {
  auto controller = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService");
  auto controller2 = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService2");
  auto controller3 = std::make_shared<minifi::controllers::NetworkPrioritizerService>("TestService3");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  controller->initialize();
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");

  controller3->initialize();
  controller3->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth0");
  controller3->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller3->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "1 kB");

  controller3->onEnable();

  controller2->initialize();
  controller2->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth1");
  controller2->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller2->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "10 B");
  controller3->setProperty(minifi::controllers::NetworkPrioritizerService::MaxPayload, "10 B");
  controller2->onEnable();
  std::vector<std::shared_ptr<core::controller::ControllerService> > services;
  services.push_back(controller2);
  services.push_back(controller3);
  controller->setLinkedControllerServices(services);
  controller->onEnable();
  // can't because we've triggered the max payload
  REQUIRE("eth0" == controller->getInterface(50).getInterface());
  REQUIRE("eth0" == controller->getInterface(50).getInterface());
}
