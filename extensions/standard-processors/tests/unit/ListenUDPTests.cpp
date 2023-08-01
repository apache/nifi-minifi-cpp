/**
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
#include <string>

#include "Catch.h"
#include "processors/ListenUDP.h"
#include "SingleProcessorTestController.h"
#include "Utils.h"
#include "controllers/SSLContextService.h"
#include "range/v3/algorithm/contains.hpp"

using ListenUDP = org::apache::nifi::minifi::processors::ListenUDP;

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

void check_for_attributes(core::FlowFile& flow_file, uint16_t port) {
  const auto local_addresses = {"127.0.0.1", "::ffff:127.0.0.1", "::1"};
  CHECK(std::to_string(port) == flow_file.getAttribute("udp.port"));
  CHECK(ranges::contains(local_addresses, flow_file.getAttribute("udp.sender")));
}

TEST_CASE("ListenUDP test multiple messages", "[ListenUDP][NetworkListenerProcessor]") {
  const auto listen_udp = std::make_shared<ListenUDP>("ListenUDP");
  SingleProcessorTestController controller{listen_udp};
  LogTestController::getInstance().setTrace<ListenUDP>();

  REQUIRE(listen_udp->setProperty(ListenUDP::MaxBatchSize, "2"));

  auto port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_udp);

  asio::ip::udp::endpoint endpoint;
  SECTION("sending through IPv6", "[IPv6]") {
    if (utils::isIPv6Disabled())
      SKIP("IPv6 is disabled");
    endpoint = asio::ip::udp::endpoint(asio::ip::address_v6::loopback(), port);
  }
  SECTION("sending through IPv4", "[IPv4]") {
    endpoint = asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), port);
  }

  controller.plan->scheduleProcessor(listen_udp);
  CHECK_THAT(utils::sendUdpDatagram({"test_message_1"}, endpoint), MatchesSuccess());
  CHECK_THAT(utils::sendUdpDatagram({"another_message"}, endpoint), MatchesSuccess());
  ProcessorTriggerResult result;
  REQUIRE(controller.triggerUntil({{ListenUDP::Success, 2}}, result, 300ms, 50ms));
  CHECK(result.at(ListenUDP::Success).size() == 2);
  CHECK(controller.plan->getContent(result.at(ListenUDP::Success)[0]) == "test_message_1");
  CHECK(controller.plan->getContent(result.at(ListenUDP::Success)[1]) == "another_message");

  check_for_attributes(*result.at(ListenUDP::Success)[0], port);
  check_for_attributes(*result.at(ListenUDP::Success)[1], port);
}

TEST_CASE("ListenUDP can be rescheduled", "[ListenUDP][NetworkListenerProcessor]") {
  const auto listen_udp = std::make_shared<ListenUDP>("ListenUDP");
  SingleProcessorTestController controller{listen_udp};
  LogTestController::getInstance().setTrace<ListenUDP>();
  REQUIRE(listen_udp->setProperty(ListenUDP::Port, "0"));
  REQUIRE(listen_udp->setProperty(ListenUDP::MaxBatchSize, "100"));

  REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_udp));
  REQUIRE_NOTHROW(controller.plan->reset(true));
  REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_udp));
}

TEST_CASE("ListenUDP max queue and max batch size test", "[ListenUDP][NetworkListenerProcessor]") {
  const auto listen_udp = std::make_shared<ListenUDP>("ListenUDP");
  SingleProcessorTestController controller{listen_udp};
  REQUIRE(listen_udp->setProperty(ListenUDP::MaxBatchSize, "10"));
  REQUIRE(listen_udp->setProperty(ListenUDP::MaxQueueSize, "50"));

  auto port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_udp);

  asio::ip::udp::endpoint endpoint;
  SECTION("sending through IPv6", "[IPv6]") {
    if (utils::isIPv6Disabled())
      SKIP("IPv6 is disabled");
    endpoint = asio::ip::udp::endpoint(asio::ip::address_v6::loopback(), port);
  }
  SECTION("sending through IPv4", "[IPv4]") {
    endpoint = asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), port);
  }

  LogTestController::getInstance().setWarn<ListenUDP>();

  controller.plan->scheduleProcessor(listen_udp);
  for (auto i = 0; i < 100; ++i) {
    CHECK_THAT(utils::sendUdpDatagram({"test_message"}, endpoint), MatchesSuccess());
  }

  CHECK(utils::countLogOccurrencesUntil("Queue is full. UDP message ignored.", 50, 300ms, 50ms));
  CHECK(controller.trigger().at(ListenUDP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenUDP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenUDP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenUDP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenUDP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenUDP::Success).empty());
}

}  // namespace org::apache::nifi::minifi::test
