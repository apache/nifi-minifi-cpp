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
#include "SingleProcessorTestController.h"
#include "Catch.h"
#include "PutUDP.h"
#include "core/ProcessContext.h"
#include "utils/net/UdpServer.h"
#include "utils/expected.h"
#include "utils/StringUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

namespace {
std::optional<utils::net::Message> tryDequeueWithTimeout(utils::net::UdpServer& listener, std::chrono::milliseconds timeout = 200ms, std::chrono::milliseconds interval = 10ms) {
  auto start_time = std::chrono::system_clock::now();
  utils::net::Message result;
  while (start_time + timeout > std::chrono::system_clock::now()) {
    if (listener.tryDequeue(result))
      return result;
    std::this_thread::sleep_for(interval);
  }
  return std::nullopt;
}
}  // namespace

TEST_CASE("PutUDP", "[putudp]") {
  const auto put_udp = std::make_shared<PutUDP>("PutUDP");
  auto random_engine = std::mt19937{std::random_device{}()};  // NOLINT: "Missing space before {  [whitespace/braces] [5]"
  // most systems use ports 32768 - 65535 as ephemeral ports, so avoid binding to those
  const auto port = std::uniform_int_distribution<uint16_t>{10000, 32768 - 1}(random_engine);

  test::SingleProcessorTestController controller{put_udp};
  LogTestController::getInstance().setTrace<PutUDP>();
  LogTestController::getInstance().setTrace<core::ProcessContext>();
  put_udp->setProperty(PutUDP::Hostname, "${literal('localhost')}");
  put_udp->setProperty(PutUDP::Port, utils::StringUtils::join_pack("${literal('", std::to_string(port), "')}"));

  utils::net::UdpServer listener{std::nullopt, port, core::logging::LoggerFactory<utils::net::UdpServer>::getLogger()};

  auto server_thread = std::thread([&listener]() { listener.run(); });
  auto cleanup_server = gsl::finally([&]{
    listener.stop();
    server_thread.join();
  });

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
    CHECK(!received_message->sender_address.to_string().empty());
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
    CHECK(!received_message->sender_address.to_string().empty());
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
    controller.plan->setProperty(put_udp, PutUDP::Hostname.getName(), "invalid_hostname");
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
