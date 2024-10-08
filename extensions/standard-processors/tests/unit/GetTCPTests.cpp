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
#include "processors/GetTCP.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestUtils.h"
#include "utils/net/AsioCoro.h"
#include "utils/net/AsioSocketUtils.h"
#include "controllers/SSLContextService.h"
#include "range/v3/algorithm/contains.hpp"
#include "utils/gsl.h"

using GetTCP = org::apache::nifi::minifi::processors::GetTCP;

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

void check_for_attributes(core::FlowFile& flow_file, uint16_t port) {
  const auto local_addresses = {"127.0.0.1:" + std::to_string(port), "::ffff:127.0.0.1:" + std::to_string(port), "::1:" + std::to_string(port)};
  CHECK(ranges::contains(local_addresses, flow_file.getAttribute(GetTCP::SourceEndpoint.name)));
}

minifi::utils::net::SslData createSslDataForServer() {
  const std::filesystem::path executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  minifi::utils::net::SslData ssl_data;
  ssl_data.ca_loc = (executable_dir / "resources" / "ca_A.crt").string();
  ssl_data.cert_loc = (executable_dir / "resources" / "localhost_by_A.pem").string();
  ssl_data.key_loc = (executable_dir / "resources" / "localhost.key").string();
  return ssl_data;
}

void addSslContextServiceTo(SingleProcessorTestController& controller) {
  auto ssl_context_service = controller.plan->addController("SSLContextService", "SSLContextService");
  LogTestController::getInstance().setTrace<GetTCP>();
  const auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextServiceImpl::CACertificate, (executable_dir / "resources" / "ca_A.crt").string()));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextServiceImpl::ClientCertificate, (executable_dir / "resources" / "alice_by_A.pem").string()));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextServiceImpl::PrivateKey, (executable_dir / "resources" / "alice.key").string()));
  ssl_context_service->enable();
}

class TcpTestServer {
 public:
  void run() {
    server_thread_ = std::thread([&]() {
      asio::co_spawn(io_context_, listenAndSendMessages(), asio::detached);
      io_context_.run();
    });
  }

  void queueMessage(std::string message) {
    messages_to_send_.enqueue(std::move(message));
  }

  void enableSSL() {
    const std::filesystem::path executable_dir = minifi::utils::file::FileUtils::get_executable_dir();

    asio::ssl::context ssl_context(asio::ssl::context::tls_server);
    ssl_context.set_options(minifi::utils::net::MINIFI_SSL_OPTIONS);
    ssl_context.set_password_callback([key_pw = "Password12"](std::size_t&, asio::ssl::context_base::password_purpose&) { return key_pw; });
    ssl_context.use_certificate_file((executable_dir / "resources" / "localhost_by_A.pem").string(), asio::ssl::context::pem);
    ssl_context.use_private_key_file((executable_dir / "resources" / "localhost.key").string(), asio::ssl::context::pem);
    ssl_context.load_verify_file((executable_dir / "resources" / "ca_A.crt").string());
    ssl_context.set_verify_mode(asio::ssl::verify_peer);

    ssl_context_ = std::move(ssl_context);
  }

  uint16_t getPort() const {
    return port_;
  }

  TcpTestServer() = default;
  TcpTestServer(TcpTestServer&&) = delete;
  TcpTestServer(const TcpTestServer&) = delete;
  TcpTestServer& operator=(TcpTestServer&&) = delete;
  TcpTestServer& operator=(const TcpTestServer&) = delete;

  ~TcpTestServer() {
    io_context_.stop();
    if (server_thread_.joinable())
      server_thread_.join();
  }

 private:
  asio::awaitable<void> sendMessages(auto& socket) {
    while (true) {
      std::string message_to_send;
      if (!messages_to_send_.tryDequeue(message_to_send)) {
        co_await minifi::utils::net::async_wait(10ms);
        continue;
      }
      co_await asio::async_write(socket, asio::buffer(message_to_send), minifi::utils::net::use_nothrow_awaitable);
    }
  }

  asio::awaitable<void> secureSession(asio::ip::tcp::socket socket) {
    gsl_Expects(ssl_context_);
    minifi::utils::net::SslSocket ssl_socket(std::move(socket), *ssl_context_);
    auto [handshake_error] = co_await ssl_socket.async_handshake(minifi::utils::net::HandshakeType::server, minifi::utils::net::use_nothrow_awaitable);
    if (handshake_error) {
      co_return;
    }
    co_await sendMessages(ssl_socket);
    asio::error_code ec;
    ssl_socket.lowest_layer().cancel(ec);
    co_await ssl_socket.async_shutdown(minifi::utils::net::use_nothrow_awaitable);
  }

  asio::awaitable<void> insecureSession(asio::ip::tcp::socket socket) {
    co_await sendMessages(socket);
  }

  asio::awaitable<void> listenAndSendMessages() {
    asio::ip::tcp::acceptor acceptor(io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port_));
    if (port_ == 0)
      port_ = acceptor.local_endpoint().port();
    while (true) {
      auto [accept_error, socket] = co_await acceptor.async_accept(minifi::utils::net::use_nothrow_awaitable);
      if (accept_error) {
        co_return;
      }
      if (ssl_context_)
        co_spawn(io_context_, secureSession(std::move(socket)), asio::detached);
      else
        co_spawn(io_context_, insecureSession(std::move(socket)), asio::detached);
    }
  }

  std::optional<asio::ssl::context> ssl_context_;
  minifi::utils::ConcurrentQueue<std::string> messages_to_send_;
  std::atomic<uint16_t> port_ = 0;
  std::thread server_thread_;
  asio::io_context io_context_;
};

TEST_CASE("GetTCP test with delimiter", "[GetTCP]") {
  const auto get_tcp = std::make_shared<GetTCP>("GetTCP");
  SingleProcessorTestController controller{get_tcp};
  LogTestController::getInstance().setTrace<GetTCP>();
  REQUIRE(get_tcp->setProperty(GetTCP::MaxBatchSize, "2"));


  TcpTestServer tcp_test_server;

  SECTION("No SSL") {}

  SECTION("SSL") {
    addSslContextServiceTo(controller);
    tcp_test_server.enableSSL();
    REQUIRE(get_tcp->setProperty(GetTCP::SSLContextService, "SSLContextService"));
  }

  tcp_test_server.queueMessage("Hello\n");
  tcp_test_server.run();
  REQUIRE(utils::verifyEventHappenedInPollTime(250ms, [&] { return tcp_test_server.getPort() != 0; }, 20ms));

  REQUIRE(get_tcp->setProperty(GetTCP::EndpointList, fmt::format("localhost:{}", tcp_test_server.getPort())));
  controller.plan->scheduleProcessor(get_tcp);

  ProcessorTriggerResult result;
  REQUIRE(controller.triggerUntil({{GetTCP::Success, 1}}, result, 1s, 50ms));
  CHECK(controller.plan->getContent(result.at(GetTCP::Success)[0]) == "Hello\n");

  check_for_attributes(*result.at(GetTCP::Success)[0], tcp_test_server.getPort());
}

TEST_CASE("GetTCP test with too large message", "[GetTCP]") {
  const auto get_tcp = std::make_shared<GetTCP>("GetTCP");
  SingleProcessorTestController controller{get_tcp};
  LogTestController::getInstance().setTrace<GetTCP>();
  REQUIRE(get_tcp->setProperty(GetTCP::MaxBatchSize, "2"));
  REQUIRE(get_tcp->setProperty(GetTCP::MaxMessageSize, "10"));
  REQUIRE(get_tcp->setProperty(GetTCP::MessageDelimiter, "\r"));

  TcpTestServer tcp_test_server;

  SECTION("No SSL") {}

  SECTION("SSL") {
    addSslContextServiceTo(controller);
    tcp_test_server.enableSSL();
    REQUIRE(get_tcp->setProperty(GetTCP::SSLContextService, "SSLContextService"));
  }

  tcp_test_server.queueMessage("abcdefghijklmnopqrstuvwxyz\rBye\r");
  tcp_test_server.run();

  REQUIRE(utils::verifyEventHappenedInPollTime(250ms, [&] { return tcp_test_server.getPort() != 0; }, 20ms));

  REQUIRE(get_tcp->setProperty(GetTCP::EndpointList, fmt::format("localhost:{}", tcp_test_server.getPort())));
  controller.plan->scheduleProcessor(get_tcp);

  ProcessorTriggerResult result;
  REQUIRE(controller.triggerUntil({{GetTCP::Success, 1}}, result, 1s, 50ms));
  REQUIRE(result.at(GetTCP::Partial).size() == 3);
  REQUIRE(result.at(GetTCP::Success).size() == 1);
  CHECK(controller.plan->getContent(result.at(GetTCP::Partial)[0]) == "abcdefghij");
  CHECK(controller.plan->getContent(result.at(GetTCP::Partial)[1]) == "klmnopqrst");
  CHECK(controller.plan->getContent(result.at(GetTCP::Partial)[2]) == "uvwxyz\r");
  CHECK(controller.plan->getContent(result.at(GetTCP::Success)[0]) == "Bye\r");

  check_for_attributes(*result.at(GetTCP::Partial)[0], tcp_test_server.getPort());
  check_for_attributes(*result.at(GetTCP::Partial)[1], tcp_test_server.getPort());
  check_for_attributes(*result.at(GetTCP::Partial)[2], tcp_test_server.getPort());
  check_for_attributes(*result.at(GetTCP::Success)[0], tcp_test_server.getPort());
}

TEST_CASE("GetTCP test multiple endpoints", "[GetTCP]") {
  const auto get_tcp = std::make_shared<GetTCP>("GetTCP");
  SingleProcessorTestController controller{get_tcp};
  LogTestController::getInstance().setTrace<GetTCP>();
  REQUIRE(get_tcp->setProperty(GetTCP::MaxBatchSize, "2"));

  TcpTestServer server_1;
  TcpTestServer server_2;

  SECTION("No SSL") {}

  SECTION("SSL") {
    addSslContextServiceTo(controller);
    server_1.enableSSL();
    server_2.enableSSL();
    REQUIRE(get_tcp->setProperty(GetTCP::SSLContextService, "SSLContextService"));
  }

  server_1.queueMessage("abcdefghijklmnopqrstuvwxyz\nBye\n");
  server_1.run();

  server_2.queueMessage("012345678901234567890\nAuf Wiedersehen\n");
  server_2.run();

  REQUIRE(utils::verifyEventHappenedInPollTime(250ms, [&] { return server_1.getPort() != 0 && server_2.getPort() != 0; }, 20ms));

  REQUIRE(get_tcp->setProperty(GetTCP::EndpointList, fmt::format("localhost:{},localhost:{}", server_1.getPort(), server_2.getPort())));
  controller.plan->scheduleProcessor(get_tcp);

  ProcessorTriggerResult result;
  CHECK(controller.triggerUntil({{GetTCP::Success, 4}}, result, 1s, 50ms));
  CHECK(result.at(GetTCP::Success).size() == 4);

  std::vector<std::string> success_flow_file_contents;
  for (const auto& flow_file: result.at(GetTCP::Success)) {
    success_flow_file_contents.push_back(controller.plan->getContent(flow_file));
  }

  CHECK(ranges::contains(success_flow_file_contents, "abcdefghijklmnopqrstuvwxyz\n"));
  CHECK(ranges::contains(success_flow_file_contents, "Bye\n"));
  CHECK(ranges::contains(success_flow_file_contents, "012345678901234567890\n"));
  CHECK(ranges::contains(success_flow_file_contents, "Auf Wiedersehen\n"));
}

TEST_CASE("GetTCP max queue and max batch size test", "[GetTCP]") {
  const auto get_tcp = std::make_shared<GetTCP>("GetTCP");
  SingleProcessorTestController controller{get_tcp};
  LogTestController::getInstance().setTrace<GetTCP>();
  REQUIRE(get_tcp->setProperty(GetTCP::MaxBatchSize, "10"));
  REQUIRE(get_tcp->setProperty(GetTCP::MaxQueueSize, "50"));

  TcpTestServer server;

  SECTION("No SSL") {}
  SECTION("SSL") {
    addSslContextServiceTo(controller);
    server.enableSSL();
    REQUIRE(get_tcp->setProperty(GetTCP::SSLContextService, "SSLContextService"));
  }

  LogTestController::getInstance().setWarn<GetTCP>();

  for (auto i = 0; i < 100; ++i) {
    server.queueMessage("some_message\n");
  }

  server.run();

  REQUIRE(utils::verifyEventHappenedInPollTime(250ms, [&] { return server.getPort() != 0; }, 20ms));

  REQUIRE(get_tcp->setProperty(GetTCP::EndpointList, fmt::format("localhost:{}", server.getPort())));
  controller.plan->scheduleProcessor(get_tcp);

  CHECK(utils::countLogOccurrencesUntil("Queue is full. TCP message ignored.", 50, 300ms, 50ms));
  CHECK(controller.trigger().at(GetTCP::Success).size() == 10);
  CHECK(controller.trigger().at(GetTCP::Success).size() == 10);
  CHECK(controller.trigger().at(GetTCP::Success).size() == 10);
  CHECK(controller.trigger().at(GetTCP::Success).size() == 10);
  CHECK(controller.trigger().at(GetTCP::Success).size() == 10);
  CHECK(controller.trigger().at(GetTCP::Success).empty());
}
}  // namespace org::apache::nifi::minifi::test
