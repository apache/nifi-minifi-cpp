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
#include "unit/SingleProcessorTestController.h"
#include "unit/Catch.h"
#include "PutUDP.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "utils/net/UdpServer.h"
#include "utils/expected.h"
#include "utils/StringUtils.h"
#include "unit/TestUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

namespace {
std::optional<utils::net::Message> tryDequeueWithTimeout(utils::net::UdpServer& listener, std::chrono::milliseconds timeout = 200ms, std::chrono::milliseconds interval = 10ms) {
  auto start_time = std::chrono::system_clock::now();
  while (start_time + timeout > std::chrono::system_clock::now()) {
    if (const auto result = listener.tryDequeue())
      return result;
    std::this_thread::sleep_for(interval);
  }
  return std::nullopt;
}
}  // namespace

TEST_CASE("PutUDP", "[putudp]") {
  test::SingleProcessorTestController controller{minifi::test::utils::make_processor<PutUDP>("PutUDP")};
  const auto put_udp = controller.getProcessor();

  LogTestController::getInstance().setTrace<PutUDP>();
  LogTestController::getInstance().setTrace<core::ProcessContext>();
  REQUIRE(put_udp->setProperty(PutUDP::Hostname.name, "${literal('localhost')}"));

  utils::net::UdpServer listener{std::nullopt, 0, core::logging::LoggerFactory<utils::net::UdpServer>::getLogger()};

  auto server_thread = std::thread([&listener]() { listener.run(); });
  uint16_t port = listener.getPort();
  auto deadline = std::chrono::steady_clock::now() + 200ms;
  while (port == 0 && deadline > std::chrono::steady_clock::now()) {
    std::this_thread::sleep_for(20ms);
    port = listener.getPort();
  }
  auto cleanup_server = gsl::finally([&]{
    listener.stop();
    server_thread.join();
  });
  REQUIRE(put_udp->setProperty(PutUDP::Port.name, utils::string::join_pack("${literal('", std::to_string(port), "')}")));

  {
    const char* const message = "first message: hello";
    const auto result = controller.trigger(message);
    const auto& success_flow_files = result.at(PutUDP::Success);
    REQUIRE(success_flow_files.size() == 1);
    CHECK(result.at(PutUDP::Failure).empty());
    CHECK(controller.plan->getContent(success_flow_files[0]) == message);
    auto received_message = tryDequeueWithTimeout(listener);
    REQUIRE(received_message);
    CHECK(received_message->message_data == message);
    CHECK(received_message->protocol == utils::net::IpProtocol::UDP);
    CHECK(!received_message->remote_address.to_string().empty());
  }

  {
    const char* const message = "longer message AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";  // NOLINT
    const auto result = controller.trigger(message);
    const auto& success_flow_files = result.at(PutUDP::Success);
    REQUIRE(success_flow_files.size() == 1);
    CHECK(result.at(PutUDP::Failure).empty());
    CHECK(controller.plan->getContent(success_flow_files[0]) == message);
    auto received_message = tryDequeueWithTimeout(listener);
    REQUIRE(received_message);
    CHECK(received_message->message_data == message);
    CHECK(received_message->protocol == utils::net::IpProtocol::UDP);
    CHECK(!received_message->remote_address.to_string().empty());
  }

  {
    LogTestController::getInstance().clear();
    auto message = std::string(65536, 'a');
    const auto result = controller.trigger(message);
    const auto& failure_flow_files = result.at(PutUDP::Failure);
    REQUIRE(failure_flow_files.size() == 1);
    CHECK(result.at(PutUDP::Success).empty());
    CHECK(controller.plan->getContent(failure_flow_files[0]) == message);
    CHECK((LogTestController::getInstance().contains("Message too long")
        || LogTestController::getInstance().contains("A message sent on a datagram socket was larger than the internal message buffer")));
  }

  {
    LogTestController::getInstance().clear();
    const char* const message = "message for invalid host";
    controller.plan->setProperty(put_udp, PutUDP::Hostname, "invalid_hostname");
    const auto result = controller.trigger(message);
    const auto& failure_flow_files = result.at(PutUDP::Failure);
    auto received_message = tryDequeueWithTimeout(listener);
    CHECK(!received_message);
    REQUIRE(failure_flow_files.size() == 1);
    CHECK(result.at(PutUDP::Success).empty());
    CHECK(controller.plan->getContent(failure_flow_files[0]) == message);
    CHECK((LogTestController::getInstance().contains("Host not found") || LogTestController::getInstance().contains("No such host is known")));
  }
}
}  // namespace org::apache::nifi::minifi::processors
