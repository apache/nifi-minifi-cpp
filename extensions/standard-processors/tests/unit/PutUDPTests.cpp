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
#include <unordered_map>
#include "range/v3/view/transform.hpp"
#include "range/v3/range/conversion.hpp"
#include "TestBase.h"
#include "PutUDP.h"
#include "utils/net/DNS.h"
#include "utils/net/Socket.h"
#include "utils/OptionalUtils.h"

namespace org::apache::nifi::minifi::processors {

namespace {
class SingleInputTestController : public TestController {
 public:
  explicit SingleInputTestController(const std::shared_ptr<core::Processor>& processor)
    : processor_{plan->addProcessor(processor, processor->getName())}
  {}

  std::unordered_map<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>>
      trigger(const std::string& input_flow_file_content, std::unordered_map<std::string, std::string> input_flow_file_attributes = {}) {
    const auto flow_file = createFlowFile(input_flow_file_content, std::move(input_flow_file_attributes));
    input_->put(flow_file);
    plan->runProcessor(processor_);
    return outgoing_connections_ | ranges::views::transform([](const auto& kv) -> std::pair<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>> {
      const std::shared_ptr<Connection>& connection = kv.second;
      std::set<std::shared_ptr<core::FlowFile>> expired_flow_files;
      std::vector<std::shared_ptr<core::FlowFile>> result;
      while (connection->isWorkAvailable()) {
        auto flow_file = connection->poll(expired_flow_files);
        CHECK(expired_flow_files.empty());
        if (!flow_file) continue;
        result.push_back(flow_file);
      }
      return std::make_pair(kv.first, result);
    }) | ranges::to<std::unordered_map>();
  }

 private:
  std::shared_ptr<core::FlowFile> createFlowFile(const std::string& content, std::unordered_map<std::string, std::string> attributes) {
    const auto flow_file = std::make_shared<minifi::FlowFileRecord>();
    for (auto& attr : std::move(attributes)) {
      flow_file->setAttribute(attr.first, std::move(attr.second));
    }
    auto content_session = plan->getContentRepo()->createSession();
    auto claim = content_session->create();
    auto stream = content_session->write(claim);
    stream->write(reinterpret_cast<const uint8_t*>(content.c_str()), content.length());
    flow_file->setResourceClaim(claim);
    flow_file->setSize(stream->size());
    flow_file->setOffset(0);

    stream->close();
    content_session->commit();
    return flow_file;
  }

 public:
  std::shared_ptr<TestPlan> plan = createPlan();

 private:
  std::shared_ptr<core::Processor> processor_;
  std::unordered_map<core::Relationship, std::shared_ptr<minifi::Connection>> outgoing_connections_{[this] {
    std::unordered_map<core::Relationship, std::shared_ptr<Connection>> result;
    for (const auto& relationship: processor_->getSupportedRelationships()) {
      result.insert_or_assign(relationship, plan->addConnection(processor_, relationship, nullptr));
    }
    return result;
  }()};
  std::shared_ptr<minifi::Connection> input_ = plan->addConnection(nullptr, PutUDP::Success, processor_);
};

struct DatagramListener {
  DatagramListener(const char* const hostname, const char* const port)
    :resolved_names_{utils::net::resolveHost(hostname, port, utils::net::IpProtocol::Udp)},
     open_socket_{utils::net::open_socket(resolved_names_.get())
        | utils::orElseGet([=]() -> utils::net::OpenSocketResult { throw std::runtime_error{utils::StringUtils::join_pack("Failed to connect to ", hostname, " on port ", port)}; })}
  {
    const auto bind_result = bind(open_socket_.socket_.get(), open_socket_.selected_name->ai_addr, open_socket_.selected_name->ai_addrlen);
    if (bind_result == utils::net::SocketError) {
      throw std::runtime_error{utils::StringUtils::join_pack("bind: ", utils::net::get_last_socket_error_message())};
    }
  }

  struct ReceiveResult {
    std::string remote_address;
    std::string message;
  };

  [[nodiscard]] ReceiveResult receive(const size_t max_message_size = BUFSIZ) const {
    ReceiveResult result;
    result.message.resize(max_message_size);
    sockaddr_storage remote_address{};
    socklen_t addrlen = sizeof(remote_address);
    const auto recv_result = recvfrom(open_socket_.socket_.get(), result.message.data(), result.message.size(), 0, std::launder(reinterpret_cast<sockaddr*>(&remote_address)), &addrlen);
    if (recv_result == utils::net::SocketError) {
      throw std::runtime_error{utils::StringUtils::join_pack("recvfrom: ", utils::net::get_last_socket_error_message())};
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
  auto random_engine = std::mt19937{std::random_device{}()};
  // most systems use ports 32768 - 65535 as ephemeral ports, so avoid binding to those
  const auto port = std::uniform_int_distribution<uint16_t>{10000, 32768 - 1}(random_engine);
  const auto port_str = std::to_string(port);

  LogTestController::getInstance().setTrace<PutUDP>();
  SingleInputTestController controller{putudp};
  putudp->setProperty(PutUDP::Hostname, "localhost");
  putudp->setProperty(PutUDP::Port, port_str);

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
