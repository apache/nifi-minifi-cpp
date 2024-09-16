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
#include "PutTCP.h"

#include <utility>
#include <tuple>

#include "range/v3/range/conversion.hpp"

#include "utils/gsl.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "core/logging/Logger.h"
#include "utils/net/AsioCoro.h"
#include "utils/net/AsioSocketUtils.h"

using namespace std::literals::chrono_literals;
using org::apache::nifi::minifi::utils::net::TcpSocket;
using org::apache::nifi::minifi::utils::net::SslSocket;

namespace org::apache::nifi::minifi::processors {

constexpr size_t chunk_size = 1024;

PutTCP::PutTCP(const std::string& name, const utils::Identifier& uuid)
    : ProcessorImpl(name, uuid) {}

PutTCP::~PutTCP() = default;

void PutTCP::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutTCP::notifyStop() {}

void PutTCP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  // if the required properties are missing or empty even before evaluating the EL expression, then we can throw in onSchedule, before we waste any flow files
  if (context.getProperty(Hostname).value_or(std::string{}).empty()) {
    throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "missing hostname"};
  }
  if (context.getProperty(Port).value_or(std::string{}).empty()) {
    throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "missing port"};
  }
  if (const auto idle_connection_expiration = context.getProperty<core::TimePeriodValue>(IdleConnectionExpiration); idle_connection_expiration && idle_connection_expiration->getMilliseconds() > 0ms)
    idle_connection_expiration_ = idle_connection_expiration->getMilliseconds();
  else
    idle_connection_expiration_.reset();

  if (const auto timeout = context.getProperty<core::TimePeriodValue>(Timeout); timeout && timeout->getMilliseconds() > 0ms)
    timeout_duration_ = timeout->getMilliseconds();
  else
    timeout_duration_ = 15s;

  if (context.getProperty<bool>(ConnectionPerFlowFile).value_or(false))
    connections_.reset();
  else
    connections_.emplace();

  std::string context_name;
  ssl_context_.reset();
  if (context.getProperty(SSLContextService, context_name) && !IsNullOrEmpty(context_name)) {
    if (auto controller_service = context.getControllerService(context_name)) {
      if (const auto ssl_context_service = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context.getControllerService(context_name))) {
        ssl_context_ = utils::net::getSslContext(*ssl_context_service);
      } else {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, context_name + " is not an SSL Context Service");
      }
    } else {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid controller service: " + context_name);
    }
  }

  const auto delimiter_str = context.getProperty(OutgoingMessageDelimiter).value_or(std::string{});
  delimiter_ = utils::span_to<std::vector>(as_bytes(std::span(delimiter_str)));

  if (const auto max_size_of_socket_send_buffer = context.getProperty<core::DataSizeValue>(MaxSizeOfSocketSendBuffer))
    max_size_of_socket_send_buffer_ = max_size_of_socket_send_buffer->getValue();
  else
    max_size_of_socket_send_buffer_.reset();
}

void PutTCP::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  const auto flow_file = session.get();
  if (!flow_file) {
    yield();
    return;
  }

  removeExpiredConnections();

  auto hostname = context.getProperty(Hostname, flow_file.get()).value_or(std::string{});
  auto port = context.getProperty(Port, flow_file.get()).value_or(std::string{});
  if (hostname.empty() || port.empty()) {
    logger_->log_error("[{}] invalid target endpoint: hostname: {}, port: {}", flow_file->getUUIDStr(),
        hostname.empty() ? "(empty)" : hostname.c_str(),
        port.empty() ? "(empty)" : port.c_str());
    session.transfer(flow_file, Failure);
    return;
  }

  auto connection_id = utils::net::ConnectionId(std::move(hostname), std::move(port));
  std::shared_ptr<utils::net::ConnectionHandlerBase> handler;
  if (!connections_ || !connections_->contains(connection_id)) {
    if (ssl_context_)
      handler = std::make_shared<utils::net::ConnectionHandler<SslSocket>>(connection_id, timeout_duration_, logger_, max_size_of_socket_send_buffer_, &*ssl_context_);
    else
      handler = std::make_shared<utils::net::ConnectionHandler<TcpSocket>>(connection_id, timeout_duration_, logger_, max_size_of_socket_send_buffer_, nullptr);
    if (connections_)
      (*connections_)[connection_id] = handler;
  } else {
    handler = (*connections_)[connection_id];
  }

  gsl_Expects(handler);

  processFlowFile(handler, session, flow_file);
}

void PutTCP::removeExpiredConnections() {
  if (connections_) {
    std::erase_if(*connections_, [this](auto& item) -> bool {
      const auto& connection_handler = item.second;
      return (!connection_handler || (idle_connection_expiration_ && !connection_handler->hasBeenUsedIn(*idle_connection_expiration_)));
    });
  }
}

std::error_code PutTCP::sendFlowFileContent(const std::shared_ptr<utils::net::ConnectionHandlerBase>& connection_handler,
    const std::shared_ptr<io::InputStream>& flow_file_content_stream) {
  std::error_code operation_error;
  io_context_.restart();
  asio::co_spawn(io_context_,
    sendStreamWithDelimiter(*connection_handler, flow_file_content_stream, delimiter_),
    [&operation_error](const std::exception_ptr&, const std::error_code error_code) {
      operation_error = error_code;
    });
  io_context_.run();
  return operation_error;
}

asio::awaitable<std::error_code> PutTCP::sendStreamWithDelimiter(utils::net::ConnectionHandlerBase& connection_handler,
    const std::shared_ptr<io::InputStream>& stream_to_send, const std::vector<std::byte>& delimiter) {
  if (auto connection_error = co_await connection_handler.setupUsableSocket(io_context_)) {  // NOLINT (clang tidy doesnt like coroutines)
    co_return connection_error;
  }

  std::vector<std::byte> data_chunk;
  data_chunk.resize(chunk_size);
  const std::span<std::byte> buffer{data_chunk};
  while (stream_to_send->tell() < stream_to_send->size()) {
    const size_t num_read = stream_to_send->read(buffer);
    if (io::isError(num_read))
      co_return std::make_error_code(std::errc::io_error);
    auto [write_error, bytes_written] = co_await connection_handler.write(asio::buffer(data_chunk, num_read));
    if (write_error)
      co_return write_error;
    logger_->log_trace("Writing flowfile({} bytes) to socket succeeded", bytes_written);
  }
  auto [delimiter_write_error, delimiter_bytes_written] = co_await connection_handler.write(asio::buffer(delimiter));
  if (delimiter_write_error)
    co_return delimiter_write_error;
  logger_->log_trace("Writing delimiter({} bytes) to socket succeeded", delimiter_bytes_written);

  co_return std::error_code();
}

void PutTCP::processFlowFile(const std::shared_ptr<utils::net::ConnectionHandlerBase>& connection_handler,
    core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file) {
  const auto flow_file_content_stream = session.getFlowFileContentStream(*flow_file);
  if (!flow_file_content_stream) {
    session.transfer(flow_file, Failure);
    return;
  }

  std::error_code operation_error = sendFlowFileContent(connection_handler, flow_file_content_stream);

  if (operation_error && connection_handler->hasBeenUsed()) {
    logger_->log_warn("{} with reused connection, retrying...", operation_error.message());
    connection_handler->reset();
    operation_error = sendFlowFileContent(connection_handler, flow_file_content_stream);
  }

  if (operation_error) {
    connection_handler->reset();
    logger_->log_error("{}", operation_error.message());
    session.transfer(flow_file, Failure);
  } else {
    session.transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(PutTCP, Processor);

}  // namespace org::apache::nifi::minifi::processors
