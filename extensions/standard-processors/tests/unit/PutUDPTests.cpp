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

#include <memory>
#include <new>
#include <random>
#include <string>
#include "SingleInputTestController.h"
#include "PutUDP.h"
#include "utils/net/DNS.h"
#include "utils/net/Socket.h"
#include "utils/expected.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::processors {

namespace {
struct DatagramListener {
  DatagramListener(const char* const hostname, const char* const port)
    :resolved_names_{utils::net::resolveHost(hostname, port, utils::net::IpProtocol::Udp).value()},
     open_socket_{utils::net::open_socket(*resolved_names_)
        | utils::valueOrElse([=]() -> utils::net::OpenSocketResult { throw std::runtime_error{utils::StringUtils::join_pack("Failed to connect to ", hostname, " on port ", port)}; })}
  {
    const auto bind_result = bind(open_socket_.socket_.get(), open_socket_.selected_name->ai_addr, open_socket_.selected_name->ai_addrlen);
    if (bind_result == utils::net::SocketError) {
      throw std::runtime_error{utils::StringUtils::join_pack("bind: ", utils::net::get_last_socket_error().message())};
    }
  }

  struct ReceiveResult {
    std::string remote_address;
    std::string message;
  };

  [[nodiscard]] ReceiveResult receive(const size_t max_message_size = 8192) const {
    ReceiveResult result;
    result.message.resize(max_message_size);
    sockaddr_storage remote_address{};
    socklen_t addrlen = sizeof(remote_address);
    const auto recv_result = recvfrom(open_socket_.socket_.get(), result.message.data(), result.message.size(), 0, std::launder(reinterpret_cast<sockaddr*>(&remote_address)), &addrlen);
    if (recv_result == utils::net::SocketError) {
      throw std::runtime_error{utils::StringUtils::join_pack("recvfrom: ", utils::net::get_last_socket_error().message())};
    }
    result.message.resize(gsl::narrow<size_t>(recv_result));
    result.remote_address = utils::net::sockaddr_ntop(std::launder(reinterpret_cast<sockaddr*>(&remote_address)));
    return result;
  }

  std::unique_ptr<addrinfo, utils::net::addrinfo_deleter> resolved_names_;
  utils::net::OpenSocketResult open_socket_;
};
}  // namespace

// Testing the failure relationship is not required, because since UDP in general without guarantees, flow files are always routed to success, unless there is
// some weird IO error with the content repo.
TEST_CASE("PutUDP", "[putudp]") {
  const auto putudp = std::make_shared<PutUDP>("PutUDP");
  auto random_engine = std::mt19937{std::random_device{}()};  // NOLINT: "Missing space before {  [whitespace/braces] [5]"
  // most systems use ports 32768 - 65535 as ephemeral ports, so avoid binding to those
  const auto port = std::uniform_int_distribution<uint16_t>{10000, 32768 - 1}(random_engine);
  const auto port_str = std::to_string(port);

  test::SingleInputTestController controller{putudp};
  LogTestController::getInstance().setTrace<PutUDP>();
  LogTestController::getInstance().setTrace<core::ProcessContext>();
  LogTestController::getInstance().setLevelByClassName(spdlog::level::trace, "org::apache::nifi::minifi::core::ProcessContextExpr");
  putudp->setProperty(PutUDP::Hostname, "${literal('localhost')}");
  putudp->setProperty(PutUDP::Port, utils::StringUtils::join_pack("${literal('", port_str, "')}"));

  DatagramListener listener{"localhost", port_str.c_str()};

  {
    const char* const message = "first message: hello";
    const auto result = controller.trigger(message);
    const auto& success_flow_files = result.at(PutUDP::Success);
    REQUIRE(success_flow_files.size() == 1);
    REQUIRE(result.at(PutUDP::Failure).empty());
    REQUIRE(controller.plan->getContent(success_flow_files[0]) == message);
    auto receive_result = listener.receive();
    REQUIRE(receive_result.message == message);
    REQUIRE(!receive_result.remote_address.empty());
  }

  {
    const char* const message = "longer message AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";  // NOLINT
    const auto result = controller.trigger(message);
    const auto& success_flow_files = result.at(PutUDP::Success);
    REQUIRE(success_flow_files.size() == 1);
    REQUIRE(result.at(PutUDP::Failure).empty());
    REQUIRE(controller.plan->getContent(success_flow_files[0]) == message);
    auto receive_result = listener.receive();
    REQUIRE(receive_result.message == message);
    REQUIRE(!receive_result.remote_address.empty());
  }
}

}  // namespace org::apache::nifi::minifi::processors
