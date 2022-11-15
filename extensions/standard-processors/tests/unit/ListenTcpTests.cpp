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
#include "processors/ListenTCP.h"
#include "SingleProcessorTestController.h"
#include "Utils.h"
#include "controllers/SSLContextService.h"
#include "range/v3/algorithm/contains.hpp"

using ListenTCP = org::apache::nifi::minifi::processors::ListenTCP;

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

constexpr uint64_t PORT = 10254;

void check_for_attributes(core::FlowFile& flow_file) {
  CHECK(std::to_string(PORT) == flow_file.getAttribute("tcp.port"));
  const auto local_addresses = {"127.0.0.1", "::ffff:127.0.0.1", "::1"};
  CHECK(ranges::contains(local_addresses, flow_file.getAttribute("tcp.sender")));
}

TEST_CASE("ListenTCP test multiple messages", "[ListenTCP][NetworkListenerProcessor]") {
  asio::ip::tcp::endpoint endpoint;
  SECTION("sending through IPv4", "[IPv4]") {
    endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), PORT);
  }
  SECTION("sending through IPv6", "[IPv6]") {
    if (utils::isIPv6Disabled())
      return;
    endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), PORT);
  }
  const auto listen_tcp = std::make_shared<ListenTCP>("ListenTCP");

  SingleProcessorTestController controller{listen_tcp};
  LogTestController::getInstance().setTrace<ListenTCP>();
  REQUIRE(listen_tcp->setProperty(ListenTCP::Port, std::to_string(PORT)));
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxBatchSize, "2"));

  controller.plan->scheduleProcessor(listen_tcp);
  REQUIRE(utils::sendMessagesViaTCP({"test_message_1"}, endpoint));
  REQUIRE(utils::sendMessagesViaTCP({"another_message"}, endpoint));
  ProcessorTriggerResult result;
  REQUIRE(controller.triggerUntil({{ListenTCP::Success, 2}}, result, 300s, 50ms));
  CHECK(controller.plan->getContent(result.at(ListenTCP::Success)[0]) == "test_message_1");
  CHECK(controller.plan->getContent(result.at(ListenTCP::Success)[1]) == "another_message");

  check_for_attributes(*result.at(ListenTCP::Success)[0]);
  check_for_attributes(*result.at(ListenTCP::Success)[1]);
}

TEST_CASE("ListenTCP can be rescheduled", "[ListenTCP][NetworkListenerProcessor]") {
  const auto listen_tcp = std::make_shared<ListenTCP>("ListenTCP");
  SingleProcessorTestController controller{listen_tcp};
  LogTestController::getInstance().setTrace<ListenTCP>();
  REQUIRE(listen_tcp->setProperty(ListenTCP::Port, std::to_string(PORT)));
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxBatchSize, "100"));

  REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_tcp));
  REQUIRE_NOTHROW(controller.plan->reset(true));
  REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_tcp));
}

TEST_CASE("ListenTCP max queue and max batch size test", "[ListenTCP][NetworkListenerProcessor]") {
  asio::ip::tcp::endpoint endpoint;
  SECTION("sending through IPv4", "[IPv4]") {
    endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), PORT);
  }
  SECTION("sending through IPv6", "[IPv6]") {
    if (utils::isIPv6Disabled())
      return;
    endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), PORT);
  }
  const auto listen_tcp = std::make_shared<ListenTCP>("ListenTCP");

  SingleProcessorTestController controller{listen_tcp};
  REQUIRE(listen_tcp->setProperty(ListenTCP::Port, std::to_string(PORT)));
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxBatchSize, "10"));
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxQueueSize, "50"));

  LogTestController::getInstance().setWarn<ListenTCP>();

  controller.plan->scheduleProcessor(listen_tcp);
  for (auto i = 0; i < 100; ++i) {
    REQUIRE(utils::sendMessagesViaTCP({"test_message"}, endpoint));
  }

  CHECK(utils::countLogOccurrencesUntil("Queue is full. TCP message ignored.", 50, 300ms, 50ms));
  CHECK(controller.trigger().at(ListenTCP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenTCP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenTCP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenTCP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenTCP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenTCP::Success).empty());
}

TEST_CASE("Test ListenTCP with SSL connection", "[ListenTCP][NetworkListenerProcessor]") {
  const auto listen_tcp = std::make_shared<ListenTCP>("ListenTCP");

  SingleProcessorTestController controller{listen_tcp};
  auto ssl_context_service = controller.plan->addController("SSLContextService", "SSLContextService");
  LogTestController::getInstance().setTrace<ListenTCP>();
  const auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::CACertificate.getName(),
      minifi::utils::file::concat_path(executable_dir, "resources/ca_A.crt")));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::ClientCertificate.getName(),
      minifi::utils::file::concat_path(executable_dir, "resources/localhost_by_A.pem")));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::PrivateKey.getName(),
      minifi::utils::file::concat_path(executable_dir, "resources/localhost_by_A.pem")));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::Passphrase.getName(), "Password12"));
  REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::Port.getName(), std::to_string(PORT)));
  REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::MaxBatchSize.getName(), "2"));
  REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::SSLContextService.getName(), "SSLContextService"));
  std::vector<std::string> expected_successful_messages;

  asio::ip::tcp::endpoint endpoint;

  SECTION("Without client certificate verification") {
    SECTION("Client certificate not required, Client Auth set to NONE by default") {
      SECTION("sending through IPv4", "[IPv4]") {
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), PORT);
      }
      SECTION("sending through IPv6", "[IPv6]") {
        if (utils::isIPv6Disabled())
          return;
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), PORT);
      }
    }
    SECTION("Client certificate not required, but validated if provided") {
      REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::ClientAuth.getName(), "WANT"));
      SECTION("sending through IPv4", "[IPv4]") {
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), PORT);
      }
      SECTION("sending through IPv6", "[IPv6]") {
        if (utils::isIPv6Disabled())
          return;
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), PORT);
      }
    }
    ssl_context_service->enable();
    controller.plan->scheduleProcessor(listen_tcp);

    expected_successful_messages = {"test_message_1", "another_message"};
    for (const auto& message: expected_successful_messages) {
      REQUIRE(utils::sendMessagesViaSSL({message}, endpoint, minifi::utils::file::concat_path(executable_dir, "resources/ca_A.crt")));
    }
  }

  SECTION("With client certificate provided") {
    SECTION("Client certificate required") {
      REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::ClientAuth.getName(), "REQUIRED"));
      SECTION("sending through IPv4", "[IPv4]") {
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), PORT);
      }
      SECTION("sending through IPv6", "[IPv6]") {
        if (utils::isIPv6Disabled())
          return;
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), PORT);
      }
    }
    SECTION("Client certificate not required but validated") {
      REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::ClientAuth.getName(), "WANT"));
      SECTION("sending through IPv4", "[IPv4]") {
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), PORT);
      }
      SECTION("sending through IPv6", "[IPv6]") {
        if (utils::isIPv6Disabled())
          return;
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), PORT);
      }
    }
    ssl_context_service->enable();
    controller.plan->scheduleProcessor(listen_tcp);

    minifi::utils::net::SslData ssl_data;
    ssl_data.ca_loc = minifi::utils::file::FileUtils::get_executable_dir() + "/resources/ca_A.crt";
    ssl_data.cert_loc = minifi::utils::file::FileUtils::get_executable_dir() + "/resources/localhost_by_A.pem";
    ssl_data.key_loc = minifi::utils::file::FileUtils::get_executable_dir() + "/resources/localhost_by_A.pem";
    ssl_data.key_pw = "Password12";

    expected_successful_messages = {"test_message_1", "another_message"};
    for (const auto& message : expected_successful_messages) {
      REQUIRE(utils::sendMessagesViaSSL({message}, endpoint, minifi::utils::file::FileUtils::get_executable_dir() + "/resources/ca_A.crt", ssl_data));
    }
  }

  SECTION("Required certificate not provided") {
    SECTION("sending through IPv4", "[IPv4]") {
      endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), PORT);
    }
    SECTION("sending through IPv6", "[IPv6]") {
      if (utils::isIPv6Disabled())
        return;
      endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), PORT);
    }
    REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::ClientAuth.getName(), "REQUIRED"));
    ssl_context_service->enable();
    controller.plan->scheduleProcessor(listen_tcp);

    REQUIRE_FALSE(utils::sendMessagesViaSSL({"test_message_1"}, endpoint, minifi::utils::file::concat_path(executable_dir, "/resources/ca_A.crt")));
  }

  ProcessorTriggerResult result;
  REQUIRE(controller.triggerUntil({{ListenTCP::Success, expected_successful_messages.size()}}, result, 300s, 50ms));
  for (std::size_t i = 0; i < expected_successful_messages.size(); ++i) {
    CHECK(controller.plan->getContent(result.at(ListenTCP::Success)[i]) == expected_successful_messages[i]);
    check_for_attributes(*result.at(ListenTCP::Success)[i]);
  }
}

}  // namespace org::apache::nifi::minifi::test
