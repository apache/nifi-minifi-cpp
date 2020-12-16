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
#include <vector>
#include <memory>
#include <utility>
#include <string>
#include "../TestBase.h"
#include "io/ClientSocket.h"
#include "core/controller/ControllerService.h"
#include "controllers/NetworkPrioritizerService.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

class NetworkPrioritizerServiceTestAccessor {
 public:
  static void setMockClock(minifi::controllers::NetworkPrioritizerService& service, std::function<std::chrono::milliseconds()> mock_clock) {
    service.milliseconds_since_epoch_ = std::move(mock_clock);
  }
};

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

namespace {

std::shared_ptr<minifi::controllers::NetworkPrioritizerService> createNetworkPrioritizerService(
    const std::string& name,
    std::function<std::chrono::milliseconds()> mock_clock = []{ return std::chrono::milliseconds{0}; }) {
  auto controller = std::make_shared<minifi::controllers::NetworkPrioritizerService>(name);
  minifi::controllers::NetworkPrioritizerServiceTestAccessor::setMockClock(*controller, mock_clock);
  return controller;
}

};  // namespace

TEST_CASE("TestPrioritizerOneInterface", "[test1]") {
  auto controller = createNetworkPrioritizerService("TestService");
  controller->initialize();
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth0,eth1");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "10 B");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::MaxPayload, "10 B");
  controller->onEnable();
  REQUIRE("eth0" == controller->getInterface(0).getInterface());
}

TEST_CASE("TestPrioritizerOneInterfaceMaxPayload", "[test2]") {
  auto controller = createNetworkPrioritizerService("TestService");
  controller->initialize();
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth0,eth1");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "1 kB");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::MaxPayload, "10 B");
  controller->onEnable();

  REQUIRE("eth0" == controller->getInterface(5).getInterface());
  REQUIRE("" == controller->getInterface(20).getInterface());  // larger than max payload
  REQUIRE("eth0" == controller->getInterface(5).getInterface());
}

TEST_CASE("TestPrioritizerOneInterfaceMaxThroughput", "[test3]") {
  std::chrono::milliseconds mock_time_since_epoch{0};
  const auto mock_clock = [&mock_time_since_epoch]{ return mock_time_since_epoch; };

  auto controller = createNetworkPrioritizerService("TestService", mock_clock);
  controller->initialize();
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth0,eth1");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "10 B");
  controller->onEnable();
  REQUIRE("eth0" == controller->getInterface(5).getInterface());
  REQUIRE("eth0" == controller->getInterface(5).getInterface());
  REQUIRE("" == controller->getInterface(5).getInterface());  // max throughput reached
  mock_time_since_epoch += std::chrono::milliseconds{10};   // wait for more tokens to be generated
  REQUIRE("eth0" == controller->getInterface(5).getInterface());  // now we can send again
}

TEST_CASE("TestPriorotizerMultipleInterfaces", "[test4]") {
  std::chrono::milliseconds mock_time_since_epoch{0};
  const auto mock_clock = [&mock_time_since_epoch]{ return mock_time_since_epoch; };

  auto parent_controller = createNetworkPrioritizerService("TestService", mock_clock);
  parent_controller->initialize();
  parent_controller->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");

  auto controller0 = createNetworkPrioritizerService("TestService_eth0", mock_clock);
  controller0->initialize();
  controller0->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth0");
  controller0->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller0->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "10 B");
  controller0->onEnable();

  auto controller1 = createNetworkPrioritizerService("TestService_eth1", mock_clock);
  controller1->initialize();
  controller1->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth1");
  controller1->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller1->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "10 B");
  controller1->onEnable();

  std::vector<std::shared_ptr<core::controller::ControllerService> > services;
  services.push_back(controller0);
  services.push_back(controller1);
  parent_controller->setLinkedControllerServices(services);
  parent_controller->onEnable();

  SECTION("Switch to second interface when the first is saturated") {
    REQUIRE("eth0" == parent_controller->getInterface(5).getInterface());
    REQUIRE("eth0" == parent_controller->getInterface(5).getInterface());
    // triggered the max throughput on eth0, switching to eth1
    REQUIRE("eth1" == parent_controller->getInterface(5).getInterface());
    REQUIRE("eth1" == parent_controller->getInterface(5).getInterface());
  }

  SECTION("Can keep sending on eth0 if we wait between packets") {
    for (int i = 0; i < 100; i++) {
      REQUIRE("eth0" == parent_controller->getInterface(10).getInterface());
      mock_time_since_epoch += std::chrono::milliseconds{5};
    }
  }
}

TEST_CASE("TestPriorotizerMultipleInterfacesMaxPayload", "[test5]") {
  auto parent_controller = createNetworkPrioritizerService("TestService");
  parent_controller->initialize();
  parent_controller->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");

  auto controller0 = createNetworkPrioritizerService("TestService_eth0");
  controller0->initialize();
  controller0->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth0");
  controller0->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller0->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "1 kB");
  controller0->setProperty(minifi::controllers::NetworkPrioritizerService::MaxPayload, "10 B");
  controller0->onEnable();

  auto controller1 = createNetworkPrioritizerService("TestService_eth1");
  controller1->initialize();
  controller1->setProperty(minifi::controllers::NetworkPrioritizerService::NetworkControllers, "eth1");
  controller1->setProperty(minifi::controllers::NetworkPrioritizerService::VerifyInterfaces, "false");
  controller1->setProperty(minifi::controllers::NetworkPrioritizerService::MaxThroughput, "1 kB");
  controller1->onEnable();

  std::vector<std::shared_ptr<core::controller::ControllerService> > services;
  services.push_back(controller0);
  services.push_back(controller1);
  parent_controller->setLinkedControllerServices(services);
  parent_controller->onEnable();

  REQUIRE("eth0" == parent_controller->getInterface(10).getInterface());
  REQUIRE("eth0" == parent_controller->getInterface(10).getInterface());
  REQUIRE("eth1" == parent_controller->getInterface(50).getInterface());  // larger than max payload
  REQUIRE("eth0" == parent_controller->getInterface(10).getInterface());
}
