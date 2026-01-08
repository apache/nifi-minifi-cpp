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

#include "unit/Catch.h"
#include "processors/ListenUDP.h"
#include "unit/SingleProcessorTestController.h"
#include "controllers/SSLContextService.h"
#include "range/v3/algorithm/contains.hpp"
#include "unit/TestUtils.h"

using ListenUDP = org::apache::nifi::minifi::processors::ListenUDP;

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

void check_for_attributes(core::FlowFile& flow_file, uint16_t server_port, uint16_t remote_port) {
  const auto local_addresses = {"127.0.0.1", "::ffff:127.0.0.1", "::1"};
  CHECK(std::to_string(server_port) == flow_file.getAttribute("udp.port"));
  CHECK(ranges::contains(local_addresses, flow_file.getAttribute("udp.sender")));
  CHECK(std::to_string(remote_port) == flow_file.getAttribute("udp.sender.port"));
}

TEST_CASE("ListenUDP test multiple messages", "[ListenUDP][NetworkListenerProcessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<ListenUDP>("ListenUDP")};
  const auto listen_udp = controller.getProcessor<ListenUDP>();
  LogTestController::getInstance().setTrace<ListenUDP>();

  REQUIRE(listen_udp->setProperty(ListenUDP::MaxBatchSize.name, "2"));

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
  const auto test_message_1_result = utils::sendUdpDatagram({"test_message_1"}, endpoint);
  CHECK_THAT(test_message_1_result.ec, MatchesSuccess());
  const auto another_message_result = utils::sendUdpDatagram({"another_message"}, endpoint);
  CHECK_THAT(another_message_result.ec, MatchesSuccess());
  ProcessorTriggerResult result;
  REQUIRE(controller.triggerUntil({{ListenUDP::Success, 2}}, result, 300ms, 50ms));
  CHECK(result.at(ListenUDP::Success).size() == 2);
  const auto test_message_1_flow_file = result.at(ListenUDP::Success)[0];
  CHECK(controller.plan->getContent(test_message_1_flow_file) == "test_message_1");
  const auto another_message_flow_file = result.at(ListenUDP::Success)[1];
  CHECK(controller.plan->getContent(another_message_flow_file) == "another_message");

  check_for_attributes(*test_message_1_flow_file, port, test_message_1_result.local_endpoint.value().port());
  check_for_attributes(*another_message_flow_file, port, another_message_result.local_endpoint.value().port());
}

TEST_CASE("ListenUDP can be rescheduled", "[ListenUDP][NetworkListenerProcessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<ListenUDP>("ListenUDP")};
  const auto listen_udp = controller.getProcessor();
  LogTestController::getInstance().setTrace<ListenUDP>();
  REQUIRE(listen_udp->setProperty(ListenUDP::Port.name, "0"));
  REQUIRE(listen_udp->setProperty(ListenUDP::MaxBatchSize.name, "100"));

  REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_udp));
  REQUIRE_NOTHROW(controller.plan->reset(true));
  REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_udp));
}

TEST_CASE("ListenUDP max queue and max batch size test", "[ListenUDP][NetworkListenerProcessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<ListenUDP>("ListenUDP")};
  const auto listen_udp = controller.getProcessor<ListenUDP>();
  REQUIRE(listen_udp->setProperty(ListenUDP::MaxBatchSize.name, "10"));
  REQUIRE(listen_udp->setProperty(ListenUDP::MaxQueueSize.name, "50"));

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
    CHECK_THAT(utils::sendUdpDatagram({"test_message"}, endpoint).ec, MatchesSuccess());
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
