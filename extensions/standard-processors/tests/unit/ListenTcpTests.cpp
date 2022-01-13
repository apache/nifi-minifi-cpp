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

void check_for_attributes(core::FlowFile& flow_file, uint16_t port) {
  CHECK(std::to_string(port) == flow_file.getAttribute("tcp.port"));
  const auto local_addresses = {"127.0.0.1", "::ffff:127.0.0.1", "::1"};
  CHECK(ranges::contains(local_addresses, flow_file.getAttribute("tcp.sender")));
}

TEST_CASE("ListenTCP test multiple messages", "[ListenTCP][NetworkListenerProcessor]") {
  const auto listen_tcp = std::make_shared<ListenTCP>("ListenTCP");
  SingleProcessorTestController controller{listen_tcp};
  LogTestController::getInstance().setTrace<ListenTCP>();
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxBatchSize, "2"));
  auto port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_tcp);

  asio::ip::tcp::endpoint endpoint;
  SECTION("sending through IPv4", "[IPv4]") {
    endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
  }
  SECTION("sending through IPv6", "[IPv6]") {
    if (utils::isIPv6Disabled())
      return;
    endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), port);
  }

  CHECK_THAT(utils::sendMessagesViaTCP({"test_message_1"}, endpoint), MatchesSuccess());
  CHECK_THAT(utils::sendMessagesViaTCP({"another_message"}, endpoint), MatchesSuccess());
  ProcessorTriggerResult result;
  REQUIRE(controller.triggerUntil({{ListenTCP::Success, 2}}, result, 300s, 50ms));
  CHECK(controller.plan->getContent(result.at(ListenTCP::Success)[0]) == "test_message_1");
  CHECK(controller.plan->getContent(result.at(ListenTCP::Success)[1]) == "another_message");

  check_for_attributes(*result.at(ListenTCP::Success)[0], port);
  check_for_attributes(*result.at(ListenTCP::Success)[1], port);
}

TEST_CASE("ListenTCP can be rescheduled", "[ListenTCP][NetworkListenerProcessor]") {
  const auto listen_tcp = std::make_shared<ListenTCP>("ListenTCP");
  SingleProcessorTestController controller{listen_tcp};
  LogTestController::getInstance().setTrace<ListenTCP>();
  REQUIRE(listen_tcp->setProperty(ListenTCP::Port, "0"));
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxBatchSize, "100"));

  REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_tcp));
  REQUIRE_NOTHROW(controller.plan->reset(true));
  REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_tcp));
}

TEST_CASE("ListenTCP max queue and max batch size test", "[ListenTCP][NetworkListenerProcessor]") {
  const auto listen_tcp = std::make_shared<ListenTCP>("ListenTCP");
  SingleProcessorTestController controller{listen_tcp};
  LogTestController::getInstance().setTrace<ListenTCP>();
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxBatchSize, "10"));
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxQueueSize, "50"));
  auto port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_tcp);

  asio::ip::tcp::endpoint endpoint;
  SECTION("sending through IPv4", "[IPv4]") {
    endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
  }
  SECTION("sending through IPv6", "[IPv6]") {
    if (utils::isIPv6Disabled())
      return;
    endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), port);
  }

  LogTestController::getInstance().setWarn<ListenTCP>();

  for (auto i = 0; i < 100; ++i) {
    CHECK_THAT(utils::sendMessagesViaTCP({"test_message"}, endpoint), MatchesSuccess());
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
  uint16_t port = 0;
  SingleProcessorTestController controller{listen_tcp};
  auto ssl_context_service = controller.plan->addController("SSLContextService", "SSLContextService");
  LogTestController::getInstance().setTrace<ListenTCP>();
  const auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::CACertificate.getName(), (executable_dir / "resources" / "ca_A.crt").string()));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::ClientCertificate.getName(), (executable_dir / "resources" / "localhost_by_A.pem").string()));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::PrivateKey.getName(), (executable_dir / "resources" / "localhost_by_A.pem").string()));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::Passphrase.getName(), "Password12"));
  REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::MaxBatchSize.getName(), "2"));
  REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::SSLContextService.getName(), "SSLContextService"));
  std::vector<std::string> expected_successful_messages;

  asio::ip::tcp::endpoint endpoint;

  SECTION("Without client certificate verification") {
    SECTION("Client certificate not required, Client Auth set to NONE by default") {
      ssl_context_service->enable();
      port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_tcp);
      SECTION("sending through IPv4", "[IPv4]") {
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
      }
      SECTION("sending through IPv6", "[IPv6]") {
        if (utils::isIPv6Disabled())
          return;
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), port);
      }
    }
    SECTION("Client certificate not required, but validated if provided") {
      REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::ClientAuth.getName(), "WANT"));
      ssl_context_service->enable();
      port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_tcp);
      SECTION("sending through IPv4", "[IPv4]") {
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
      }
      SECTION("sending through IPv6", "[IPv6]") {
        if (utils::isIPv6Disabled())
          return;
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), port);
      }
    }

    expected_successful_messages = {"test_message_1", "another_message"};
    for (const auto& message: expected_successful_messages) {
      CHECK_THAT(utils::sendMessagesViaSSL({message}, endpoint, executable_dir / "resources" / "ca_A.crt"), MatchesSuccess());
    }
  }

  SECTION("With client certificate provided") {
    SECTION("Client certificate required") {
      REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::ClientAuth.getName(), "REQUIRED"));
      ssl_context_service->enable();
      port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_tcp);
      SECTION("sending through IPv4", "[IPv4]") {
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
      }
      SECTION("sending through IPv6", "[IPv6]") {
        if (utils::isIPv6Disabled())
          return;
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), port);
      }
    }
    SECTION("Client certificate not required but validated") {
      REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::ClientAuth.getName(), "WANT"));
      ssl_context_service->enable();
      port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_tcp);
      SECTION("sending through IPv4", "[IPv4]") {
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
      }
      SECTION("sending through IPv6", "[IPv6]") {
        if (utils::isIPv6Disabled())
          return;
        endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), port);
      }
    }

    minifi::utils::net::SslData ssl_data;
    ssl_data.ca_loc = executable_dir / "resources" / "ca_A.crt";
    ssl_data.cert_loc = executable_dir / "resources" / "localhost_by_A.pem";
    ssl_data.key_loc = executable_dir / "resources" / "localhost_by_A.pem";
    ssl_data.key_pw = "Password12";

    expected_successful_messages = {"test_message_1", "another_message"};
    for (const auto& message : expected_successful_messages) {
      CHECK_THAT(utils::sendMessagesViaSSL({message}, endpoint, executable_dir / "resources" / "ca_A.crt", ssl_data), MatchesSuccess());
    }
  }

  SECTION("Required certificate not provided") {
    ssl_context_service->enable();
    REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::ClientAuth.getName(), "REQUIRED"));
    port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_tcp);
    SECTION("sending through IPv4", "[IPv4]") {
      endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
    }
    SECTION("sending through IPv6", "[IPv6]") {
      if (utils::isIPv6Disabled())
        return;
      endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), port);
    }

    CHECK_THAT(utils::sendMessagesViaSSL({"test_message_1"}, endpoint, executable_dir / "resources" / "ca_A.crt"), MatchesError());
  }

  ProcessorTriggerResult result;
  REQUIRE(controller.triggerUntil({{ListenTCP::Success, expected_successful_messages.size()}}, result, 300ms, 50ms));
  for (std::size_t i = 0; i < expected_successful_messages.size(); ++i) {
    CHECK(controller.plan->getContent(result.at(ListenTCP::Success)[i]) == expected_successful_messages[i]);
    check_for_attributes(*result.at(ListenTCP::Success)[i], port);
  }
}

namespace {
bool isSslMethodAvailable(asio::ssl::context::method method) {
  try {
    [[maybe_unused]] asio::ssl::context ctx(method);
    return true;
  } catch (const asio::system_error& err) {
    if (err.code() == asio::error::invalid_argument) {
      return false;
    } else {
      throw;
    }
  }
}
}  // namespace

TEST_CASE("Test ListenTCP SSL/TLS compatibility", "[ListenTCP][NetworkListenerProcessor]") {
  const auto listen_tcp = std::make_shared<ListenTCP>("ListenTCP");
  SingleProcessorTestController controller{listen_tcp};
  auto ssl_context_service = controller.plan->addController("SSLContextService", "SSLContextService");
  LogTestController::getInstance().setTrace<ListenTCP>();
  const auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::CACertificate.getName(), (executable_dir / "resources" / "ca_A.crt").string()));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::ClientCertificate.getName(), (executable_dir / "resources" / "localhost_by_A.pem").string()));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::PrivateKey.getName(), (executable_dir / "resources" / "localhost_by_A.pem").string()));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextService::Passphrase.getName(), "Password12"));
  REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::MaxBatchSize.getName(), "2"));
  REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::SSLContextService.getName(), "SSLContextService"));
  REQUIRE(controller.plan->setProperty(listen_tcp, ListenTCP::ClientAuth.getName(), "REQUIRED"));

  ssl_context_service->enable();
  uint16_t port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_tcp);
  asio::ip::tcp::endpoint endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);

  minifi::utils::net::SslData ssl_data;
  ssl_data.ca_loc = executable_dir / "resources" / "ca_A.crt";
  ssl_data.cert_loc = executable_dir / "resources" / "localhost_by_A.pem";
  ssl_data.key_loc = executable_dir / "resources" / "localhost_by_A.pem";
  ssl_data.key_pw = "Password12";


  asio::ssl::context::method client_method;
  bool expected_to_work;

  SECTION("sslv2 should be disabled") {
    client_method = asio::ssl::context::method::sslv2_client;
    expected_to_work = false;
  }

  SECTION("sslv3 should be disabled") {
    client_method = asio::ssl::context::method::sslv3_client;
    expected_to_work = false;
  }

  SECTION("tlsv11 should be disabled") {
    client_method = asio::ssl::context::method::tlsv11_client;
    expected_to_work = false;
  }

  SECTION("tlsv12 should be enabled") {
    client_method = asio::ssl::context::method::tlsv12_client;
    expected_to_work = true;
  }

  SECTION("tlsv13 should be enabled") {
    client_method = asio::ssl::context::method::tlsv13_client;
    expected_to_work = false;
  }

  if (!isSslMethodAvailable(client_method))
    return;

  auto send_result = utils::sendMessagesViaSSL({"message"}, endpoint, executable_dir / "resources" / "ca_A.crt", ssl_data, client_method);
  if (expected_to_work) {
    CHECK_THAT(send_result, MatchesSuccess());
    ProcessorTriggerResult result;
    CHECK(controller.triggerUntil({{ListenTCP::Success, 1}}, result, 300ms, 50ms));
  } else {
    CHECK_THAT(send_result, MatchesError());
    ProcessorTriggerResult result;
    CHECK_FALSE(controller.triggerUntil({{ListenTCP::Success, 1}}, result, 300ms, 50ms));
  }
}

}  // namespace org::apache::nifi::minifi::test
