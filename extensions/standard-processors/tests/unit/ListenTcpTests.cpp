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
#include "asio.hpp"

using ListenTCP = org::apache::nifi::minifi::processors::ListenTCP;

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

constexpr uint64_t PORT = 10254;
using ProcessorTriggerResult = std::unordered_map<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>>;

void sendMessagesViaTCP(const std::vector<std::string_view>& contents) {
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  asio::ip::tcp::endpoint remote_endpoint(asio::ip::address::from_string("127.0.0.1"), PORT);
  socket.connect(remote_endpoint);
  std::error_code err;
  for (auto& content : contents) {
    std::string tcp_message(content);
    tcp_message += '\n';
    socket.send(asio::buffer(tcp_message, tcp_message.size()), 0, err);
  }
  REQUIRE(!err);
  socket.close();
}

void check_for_attributes(core::FlowFile& flow_file) {
  CHECK(std::to_string(PORT) == flow_file.getAttribute("tcp.port"));
  CHECK("127.0.0.1" == flow_file.getAttribute("tcp.sender"));
}

bool triggerUntil(test::SingleProcessorTestController& controller,
                  const std::unordered_map<core::Relationship, size_t>& expected_quantities,
                  ProcessorTriggerResult& result,
                  const std::chrono::milliseconds max_duration,
                  const std::chrono::milliseconds wait_time = 50ms) {
  auto start_time = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() < start_time + max_duration) {
    for (auto& [relationship, flow_files] : controller.trigger()) {
      result[relationship].insert(result[relationship].end(), flow_files.begin(), flow_files.end());
    }
    bool expected_quantities_met = true;
    for (const auto& [relationship, expected_quantity] : expected_quantities) {
      if (result[relationship].size() < expected_quantity) {
        expected_quantities_met = false;
        break;
      }
    }
    if (expected_quantities_met)
      return true;
    std::this_thread::sleep_for(wait_time);
  }
  return false;
}

bool countLogOccurrencesUntil(const std::string& pattern,
                              const int occurrences,
                              const std::chrono::milliseconds max_duration,
                              const std::chrono::milliseconds wait_time = 50ms) {
  auto start_time = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() < start_time + max_duration) {
    if (LogTestController::getInstance().countOccurrences(pattern) == occurrences)
      return true;
    std::this_thread::sleep_for(wait_time);
  }
  return false;
}

TEST_CASE("ListenTCP test multiple messages", "[ListenTCP]") {
  const auto listen_tcp = std::make_shared<ListenTCP>("ListenTCP");

  test::SingleProcessorTestController controller{listen_tcp};
  LogTestController::getInstance().setTrace<ListenTCP>();
  REQUIRE(listen_tcp->setProperty(ListenTCP::Port, std::to_string(PORT)));
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxBatchSize, "2"));

  controller.plan->scheduleProcessor(listen_tcp);
  sendMessagesViaTCP({"test_message_1"});
  sendMessagesViaTCP({"another_message"});
  ProcessorTriggerResult result;
  REQUIRE(triggerUntil(controller, {{ListenTCP::Success, 2}}, result, 300s, 50ms));
  CHECK(controller.plan->getContent(result.at(ListenTCP::Success)[0]) == "test_message_1");
  CHECK(controller.plan->getContent(result.at(ListenTCP::Success)[1]) == "another_message");

  check_for_attributes(*result.at(ListenTCP::Success)[0]);
  check_for_attributes(*result.at(ListenTCP::Success)[1]);
}

TEST_CASE("ListenTCP can be rescheduled", "[ListenTCP]") {
  const auto listen_tcp = std::make_shared<ListenTCP>("ListenTCP");
  test::SingleProcessorTestController controller{listen_tcp};
  LogTestController::getInstance().setTrace<ListenTCP>();
  REQUIRE(listen_tcp->setProperty(ListenTCP::Port, std::to_string(PORT)));
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxBatchSize, "100"));

  REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_tcp));
  REQUIRE_NOTHROW(controller.plan->reset(true));
  REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_tcp));
}

TEST_CASE("ListenTCP max queue and max batch size test", "[ListenTCP]") {
  const auto listen_tcp = std::make_shared<ListenTCP>("ListenTCP");

  test::SingleProcessorTestController controller{listen_tcp};
  REQUIRE(listen_tcp->setProperty(ListenTCP::Port, std::to_string(PORT)));
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxBatchSize, "10"));
  REQUIRE(listen_tcp->setProperty(ListenTCP::MaxQueueSize, "50"));

  LogTestController::getInstance().setWarn<ListenTCP>();

  controller.plan->scheduleProcessor(listen_tcp);
  for (auto i = 0; i < 100; ++i) {
    sendMessagesViaTCP({"test_message"});
  }

  CHECK(countLogOccurrencesUntil("Queue is full. Syslog message ignored.", 50, 300ms, 50ms));
  CHECK(controller.trigger().at(ListenTCP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenTCP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenTCP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenTCP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenTCP::Success).size() == 10);
  CHECK(controller.trigger().at(ListenTCP::Success).empty());
}

}  // namespace org::apache::nifi::minifi::test
