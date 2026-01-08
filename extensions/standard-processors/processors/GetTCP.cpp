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
#include "GetTCP.h"


#include <memory>
#include <string>
#include <thread>

#include "asio/detached.hpp"
#include "asio/read_until.hpp"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/net/AsioCoro.h"
#include "utils/ProcessorConfigUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

void GetTCP::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}


std::vector<utils::net::ConnectionId> GetTCP::parseEndpointList(core::ProcessContext& context) {
  std::vector<utils::net::ConnectionId> connections_to_make;
  if (auto endpoint_list_str = context.getProperty(EndpointList)) {
    for (const auto& endpoint_str : utils::string::splitAndTrim(*endpoint_list_str, ",")) {
      auto hostname_service_pair = utils::string::splitAndTrim(endpoint_str, ":");
      if (hostname_service_pair.size() != 2) {
        logger_->log_error("{} endpoint is invalid, expected {{hostname}}:{{service}} format", endpoint_str);
        continue;
      }
      connections_to_make.emplace_back(hostname_service_pair[0], hostname_service_pair[1]);
    }
  }
  if (connections_to_make.empty())
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("No valid endpoint in {} property", EndpointList.name));

  return connections_to_make;
}

char GetTCP::parseDelimiter(core::ProcessContext& context) {
  char delimiter = '\n';
  if (auto delimiter_str = context.getProperty(GetTCP::MessageDelimiter)) {
    auto parsed_delimiter = utils::string::parseCharacter(*delimiter_str);
    if (!parsed_delimiter || !parsed_delimiter->has_value())
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Invalid delimiter: {} (it must be a single (escaped or not) character", *delimiter_str));
    delimiter = **parsed_delimiter;
  }
  return delimiter;
}

uint64_t GetTCP::parseMaxBatchSize(core::ProcessContext& context) {
  const auto max_batch_size = utils::parseU64Property(context, MaxBatchSize);
  if (max_batch_size == 0) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Max batch size must be greater than zero");
  }
  return max_batch_size;
}

void GetTCP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  auto connections_to_make = parseEndpointList(context);
  auto delimiter = parseDelimiter(context);
  auto ssl_context = [&]() -> std::optional<asio::ssl::context> {
    if (auto ssl_context_service = utils::parseOptionalControllerService<minifi::controllers::SSLContextServiceInterface>(context, SSLContextService, getUUID())) {
      return {utils::net::getSslContext(*ssl_context_service)};
    }
    return std::nullopt;
  }();

  std::optional<size_t> max_queue_size = utils::parseOptionalU64Property(context, MaxQueueSize);
  std::optional<size_t> max_message_size = utils::parseOptionalU64Property(context, MaxMessageSize);

  asio::steady_timer::duration timeout_duration = 1s;
  if (auto timeout_value = utils::parseOptionalDurationProperty(context, Timeout)) {
    timeout_duration = *timeout_value;
  }

  asio::steady_timer::duration reconnection_interval = 1min;
  if (auto reconnect_interval_value = utils::parseOptionalDurationProperty(context, ReconnectInterval)) {
    reconnection_interval = *reconnect_interval_value;;
  }


  client_.emplace(delimiter, timeout_duration, reconnection_interval, std::move(ssl_context), max_queue_size, max_message_size, std::move(connections_to_make), logger_);
  client_thread_ = std::thread([this]() { client_->run(); });  // NOLINT

  max_batch_size_ = parseMaxBatchSize(context);
}

void GetTCP::notifyStop() {
  if (client_)
    client_->stop();
}

void GetTCP::transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) {
  auto flow_file = session.create();
  session.writeBuffer(flow_file, message.message_data);
  flow_file->setAttribute(GetTCP::SourceEndpoint.name, fmt::format("{}:{}", message.remote_address.to_string(), std::to_string(message.remote_port)));
  if (message.is_partial)
    session.transfer(flow_file, Partial);
  else
    session.transfer(flow_file, Success);
}

void GetTCP::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  gsl_Expects(max_batch_size_ > 0);
  size_t logs_processed = 0;
  while (!client_->queueEmpty() && logs_processed < max_batch_size_) {
    if (const auto received_message = client_->tryDequeue()) {
      transferAsFlowFile(received_message.value(), session);
      ++logs_processed;
    } else {
      break;
    }
  }
}

GetTCP::TcpClient::TcpClient(char delimiter,
    asio::steady_timer::duration timeout_duration,
    asio::steady_timer::duration reconnection_interval,
    std::optional<asio::ssl::context> ssl_context,
    std::optional<size_t> max_queue_size,
    std::optional<size_t> max_message_size,
    std::vector<utils::net::ConnectionId> connections,
    std::shared_ptr<core::logging::Logger> logger)
    : delimiter_(delimiter),
      timeout_duration_(timeout_duration),
      reconnection_interval_(reconnection_interval),
      ssl_context_(std::move(ssl_context)),
      max_queue_size_(max_queue_size),
      max_message_size_(max_message_size),
      connections_(std::move(connections)),
      logger_(std::move(logger)) {
}

GetTCP::TcpClient::~TcpClient() {
  stop();
}


void GetTCP::TcpClient::run() {
  gsl_Expects(!connections_.empty());
  for (const auto& connection_id : connections_) {
    asio::co_spawn(io_context_, doReceiveFrom(connection_id), asio::detached);  // NOLINT
  }
  io_context_.run();
}

void GetTCP::TcpClient::stop() {
  io_context_.stop();
}

bool GetTCP::TcpClient::queueEmpty() const {
  return concurrent_queue_.empty();
}

std::optional<utils::net::Message> GetTCP::TcpClient::tryDequeue() {
  return concurrent_queue_.tryDequeue();
}

asio::awaitable<std::error_code> GetTCP::TcpClient::readLoop(auto& socket) {
  std::string read_message;
  bool previous_didnt_end_with_delimiter = false;
  bool current_doesnt_end_with_delimiter = false;
  while (true) {
    {
      previous_didnt_end_with_delimiter = current_doesnt_end_with_delimiter;
      current_doesnt_end_with_delimiter = false;
    }
    auto dynamic_buffer = max_message_size_ ? asio::dynamic_buffer(read_message, *max_message_size_) : asio::dynamic_buffer(read_message);
    auto [read_error, bytes_read] = co_await asio::async_read_until(socket, dynamic_buffer, delimiter_, utils::net::use_nothrow_awaitable);  // NOLINT

    if (max_message_size_ && *max_message_size_ && read_error == asio::error::not_found) {
      current_doesnt_end_with_delimiter = true;
      bytes_read = *max_message_size_;
    } else if (read_error) {
      logger_->log_error("Error during read {}", read_error.message());
      co_return read_error;
    }

    if (bytes_read == 0)
      continue;

    if (!max_queue_size_ || max_queue_size_ > concurrent_queue_.size()) {
      const auto remote_endpoint = socket.lowest_layer().remote_endpoint();
      const auto local_endpoint = socket.lowest_layer().local_endpoint();
      utils::net::Message message{read_message.substr(0, bytes_read), utils::net::IpProtocol::TCP, remote_endpoint.address(),
        remote_endpoint.port(), local_endpoint.port()};
      if (previous_didnt_end_with_delimiter || current_doesnt_end_with_delimiter)
        message.is_partial = true;
      concurrent_queue_.enqueue(std::move(message));
    } else {
      logger_->log_warn("Queue is full. TCP message ignored.");
    }
    read_message.erase(0, bytes_read);
  }
}

template<class SocketType>
asio::awaitable<std::error_code> GetTCP::TcpClient::doReceiveFromEndpoint(const asio::ip::tcp::endpoint& endpoint, SocketType& socket) {
  auto [connection_error] = co_await utils::net::asyncOperationWithTimeout(socket.lowest_layer().async_connect(endpoint, utils::net::use_nothrow_awaitable), timeout_duration_);  // NOLINT
  if (connection_error)
    co_return connection_error;
  auto [handshake_error] = co_await utils::net::handshake<SocketType>(socket, timeout_duration_);
  if (handshake_error)
    co_return handshake_error;
  co_return co_await readLoop(socket);
}

asio::awaitable<void> GetTCP::TcpClient::doReceiveFrom(const utils::net::ConnectionId& connection_id) {
  while (true) {
    asio::ip::tcp::resolver resolver(io_context_);
    auto [resolve_error, resolve_result] = co_await utils::net::asyncOperationWithTimeout(  // NOLINT
        resolver.async_resolve(connection_id.getHostname(), connection_id.getService(), utils::net::use_nothrow_awaitable), timeout_duration_);
    if (resolve_error) {
      logger_->log_error("Error during resolution: {}", resolve_error.message());
      co_await utils::net::async_wait(reconnection_interval_);
      continue;
    }

    std::error_code last_error;
    for (const auto& endpoint : resolve_result) {
      if (ssl_context_) {
        utils::net::SslSocket ssl_socket{io_context_, *ssl_context_};
        last_error = co_await doReceiveFromEndpoint<utils::net::SslSocket>(endpoint, ssl_socket);
        if (last_error)
          continue;
      } else {
        utils::net::TcpSocket tcp_socket(io_context_);
        last_error = co_await doReceiveFromEndpoint<utils::net::TcpSocket>(endpoint, tcp_socket);
        if (last_error)
          continue;
      }
    }
    logger_->log_error("Error connecting to {}:{} due to {}", connection_id.getHostname(), connection_id.getService(), last_error.message());
    co_await utils::net::async_wait(reconnection_interval_);
  }
}

REGISTER_RESOURCE(GetTCP, Processor);

}  // namespace org::apache::nifi::minifi::processors
