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
#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "core/logging/Logger.h"

#include "utils/net/AsioCoro.h"

using asio::ip::tcp;

using namespace std::literals::chrono_literals;
using std::chrono::steady_clock;
using org::apache::nifi::minifi::utils::net::use_nothrow_awaitable;
using org::apache::nifi::minifi::utils::net::HandshakeType;
using org::apache::nifi::minifi::utils::net::TcpSocket;
using org::apache::nifi::minifi::utils::net::SslSocket;
using org::apache::nifi::minifi::utils::net::asyncOperationWithTimeout;

namespace org::apache::nifi::minifi::processors {

const core::Property PutTCP::Hostname = core::PropertyBuilder::createProperty("Hostname")
    ->withDescription("The ip address or hostname of the destination.")
    ->withDefaultValue("localhost")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->build();

const core::Property PutTCP::Port = core::PropertyBuilder::createProperty("Port")
    ->withDescription("The port or service on the destination.")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->build();

const core::Property PutTCP::IdleConnectionExpiration = core::PropertyBuilder::createProperty("Idle Connection Expiration")
    ->withDescription("The amount of time a connection should be held open without being used before closing the connection. A value of 0 seconds will disable this feature.")
    ->withDefaultValue<core::TimePeriodValue>("15 seconds")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->build();

const core::Property PutTCP::Timeout = core::PropertyBuilder::createProperty("Timeout")
    ->withDescription("The timeout for connecting to and communicating with the destination.")
    ->withDefaultValue<core::TimePeriodValue>("15 seconds")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->build();

const core::Property PutTCP::ConnectionPerFlowFile = core::PropertyBuilder::createProperty("Connection Per FlowFile")
    ->withDescription("Specifies whether to send each FlowFile's content on an individual connection.")
    ->withDefaultValue(false)
    ->isRequired(true)
    ->supportsExpressionLanguage(false)
    ->build();

const core::Property PutTCP::OutgoingMessageDelimiter = core::PropertyBuilder::createProperty("Outgoing Message Delimiter")
    ->withDescription("Specifies the delimiter to use when sending messages out over the same TCP stream. "
                      "The delimiter is appended to each FlowFile message that is transmitted over the stream so that the receiver can determine when one message ends and the next message begins. "
                      "Users should ensure that the FlowFile content does not contain the delimiter character to avoid errors.")
    ->isRequired(false)
    ->supportsExpressionLanguage(true)
    ->build();

const core::Property PutTCP::SSLContextService = core::PropertyBuilder::createProperty("SSL Context Service")
    ->withDescription("The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be sent over a secure connection.")
    ->isRequired(false)
    ->asType<minifi::controllers::SSLContextService>()
    ->build();

const core::Property PutTCP::MaxSizeOfSocketSendBuffer = core::PropertyBuilder::createProperty("Max Size of Socket Send Buffer")
    ->withDescription("The maximum size of the socket send buffer that should be used. This is a suggestion to the Operating System to indicate how big the socket buffer should be.")
    ->isRequired(false)
    ->asType<core::DataSizeValue>()
    ->build();

const core::Relationship PutTCP::Success{"success", "FlowFiles that are sent to the destination are sent out this relationship."};
const core::Relationship PutTCP::Failure{"failure", "FlowFiles that encountered IO errors are send out this relationship."};

constexpr size_t chunk_size = 1024;

PutTCP::PutTCP(const std::string& name, const utils::Identifier& uuid)
    : Processor(name, uuid) {}

PutTCP::~PutTCP() = default;

void PutTCP::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void PutTCP::notifyStop() {}

namespace {
asio::ssl::context getSslContext(const controllers::SSLContextService& ssl_context_service) {
  asio::ssl::context ssl_context(asio::ssl::context::tls_client);
  ssl_context.set_options(asio::ssl::context::no_tlsv1 | asio::ssl::context::no_tlsv1_1);
  ssl_context.load_verify_file(ssl_context_service.getCACertificate().string());
  ssl_context.set_verify_mode(asio::ssl::verify_peer);
  if (const auto& cert_file = ssl_context_service.getCertificateFile(); !cert_file.empty())
    ssl_context.use_certificate_file(cert_file.string(), asio::ssl::context::pem);
  if (const auto& private_key_file = ssl_context_service.getPrivateKeyFile(); !private_key_file.empty())
    ssl_context.use_private_key_file(private_key_file.string(), asio::ssl::context::pem);
  ssl_context.set_password_callback([password = ssl_context_service.getPassphrase()](std::size_t&, asio::ssl::context_base::password_purpose&) { return password; });
  return ssl_context;
}
}  // namespace

void PutTCP::onSchedule(core::ProcessContext* const context, core::ProcessSessionFactory*) {
  gsl_Expects(context);

  // if the required properties are missing or empty even before evaluating the EL expression, then we can throw in onSchedule, before we waste any flow files
  if (context->getProperty(Hostname).value_or(std::string{}).empty()) {
    throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "missing hostname"};
  }
  if (context->getProperty(Port).value_or(std::string{}).empty()) {
    throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "missing port"};
  }
  if (auto idle_connection_expiration = context->getProperty<core::TimePeriodValue>(IdleConnectionExpiration); idle_connection_expiration && idle_connection_expiration->getMilliseconds() > 0ms)
    idle_connection_expiration_ = idle_connection_expiration->getMilliseconds();
  else
    idle_connection_expiration_.reset();

  if (auto timeout = context->getProperty<core::TimePeriodValue>(Timeout); timeout && timeout->getMilliseconds() > 0ms)
    timeout_duration_ = timeout->getMilliseconds();
  else
    timeout_duration_ = 15s;

  if (context->getProperty<bool>(ConnectionPerFlowFile).value_or(false))
    connections_.reset();
  else
    connections_.emplace();

  std::string context_name;
  ssl_context_.reset();
  if (context->getProperty(SSLContextService.getName(), context_name) && !IsNullOrEmpty(context_name)) {
    if (auto controller_service = context->getControllerService(context_name)) {
      if (auto ssl_context_service = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context->getControllerService(context_name))) {
        ssl_context_ = getSslContext(*ssl_context_service);
      } else {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, context_name + " is not an SSL Context Service");
      }
    } else {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid controller service: " + context_name);
    }
  }

  delimiter_ = utils::span_to<std::vector>(gsl::make_span(context->getProperty(OutgoingMessageDelimiter).value_or(std::string{})).as_span<const std::byte>());

  if (auto max_size_of_socket_send_buffer = context->getProperty<core::DataSizeValue>(MaxSizeOfSocketSendBuffer))
    max_size_of_socket_send_buffer_ = max_size_of_socket_send_buffer->getValue();
  else
    max_size_of_socket_send_buffer_.reset();
}

namespace {
template<class SocketType>
asio::awaitable<std::tuple<std::error_code>> handshake(SocketType&, asio::steady_timer::duration) {
  co_return std::error_code();
}

template<>
asio::awaitable<std::tuple<std::error_code>> handshake(SslSocket& socket, asio::steady_timer::duration timeout_duration) {
  co_return co_await asyncOperationWithTimeout(socket.async_handshake(HandshakeType::client, use_nothrow_awaitable), timeout_duration);  // NOLINT
}

template<class SocketType>
class ConnectionHandler : public ConnectionHandlerBase {
 public:
  ConnectionHandler(detail::ConnectionId connection_id,
                    std::chrono::milliseconds timeout,
                    std::shared_ptr<core::logging::Logger> logger,
                    std::optional<size_t> max_size_of_socket_send_buffer,
                    asio::ssl::context* ssl_context)
      : connection_id_(std::move(connection_id)),
        timeout_duration_(timeout),
        logger_(std::move(logger)),
        max_size_of_socket_send_buffer_(max_size_of_socket_send_buffer),
        ssl_context_(ssl_context) {
  }

  ~ConnectionHandler() override = default;

  asio::awaitable<std::error_code> sendStreamWithDelimiter(const std::shared_ptr<io::InputStream>& stream_to_send,
      const std::vector<std::byte>& delimiter,
      asio::io_context& io_context_) override;

 private:
  [[nodiscard]] bool hasBeenUsedIn(std::chrono::milliseconds dur) const override {
    return last_used_ && *last_used_ >= (steady_clock::now() - dur);
  }

  void reset() override {
    last_used_.reset();
    socket_.reset();
  }

  [[nodiscard]] bool hasBeenUsed() const override { return last_used_.has_value(); }
  [[nodiscard]] asio::awaitable<std::error_code> setupUsableSocket(asio::io_context& io_context);
  [[nodiscard]] bool hasUsableSocket() const { return socket_ && socket_->lowest_layer().is_open(); }

  asio::awaitable<std::error_code> establishNewConnection(const tcp::resolver::results_type& endpoints, asio::io_context& io_context_);
  asio::awaitable<std::error_code> send(const std::shared_ptr<io::InputStream>& stream_to_send, const std::vector<std::byte>& delimiter);

  SocketType createNewSocket(asio::io_context& io_context_);

  detail::ConnectionId connection_id_;
  std::optional<SocketType> socket_;

  std::optional<steady_clock::time_point> last_used_;
  std::chrono::milliseconds timeout_duration_;

  std::shared_ptr<core::logging::Logger> logger_;
  std::optional<size_t> max_size_of_socket_send_buffer_;

  asio::ssl::context* ssl_context_;
};

template<>
TcpSocket ConnectionHandler<TcpSocket>::createNewSocket(asio::io_context& io_context_) {
  gsl_Expects(!ssl_context_);
  return TcpSocket{io_context_};
}

template<>
SslSocket ConnectionHandler<SslSocket>::createNewSocket(asio::io_context& io_context_) {
  gsl_Expects(ssl_context_);
  return {io_context_, *ssl_context_};
}

template<class SocketType>
asio::awaitable<std::error_code> ConnectionHandler<SocketType>::establishNewConnection(const tcp::resolver::results_type& endpoints, asio::io_context& io_context) {
  auto socket = createNewSocket(io_context);
  std::error_code last_error;
  for (const auto& endpoint : endpoints) {
    auto [connection_error] = co_await asyncOperationWithTimeout(socket.lowest_layer().async_connect(endpoint, use_nothrow_awaitable), timeout_duration_);
    if (connection_error) {
      core::logging::LOG_DEBUG(logger_) << "Connecting to " << endpoint.endpoint() << " failed due to " << connection_error.message();
      last_error = connection_error;
      continue;
    }
    auto [handshake_error] = co_await handshake(socket, timeout_duration_);
    if (handshake_error) {
      core::logging::LOG_DEBUG(logger_) << "Handshake with " << endpoint.endpoint() << " failed due to " << handshake_error.message();
      last_error = handshake_error;
      continue;
    }
    if (max_size_of_socket_send_buffer_)
      socket.lowest_layer().set_option(TcpSocket::send_buffer_size(*max_size_of_socket_send_buffer_));
    socket_.emplace(std::move(socket));
    co_return std::error_code();
  }
  co_return last_error;
}

template<class SocketType>
[[nodiscard]] asio::awaitable<std::error_code> ConnectionHandler<SocketType>::setupUsableSocket(asio::io_context& io_context) {
  if (hasUsableSocket())
    co_return std::error_code();
  tcp::resolver resolver(io_context);
  auto [resolve_error, resolve_result] = co_await asyncOperationWithTimeout(resolver.async_resolve(connection_id_.getHostname(), connection_id_.getPort(), use_nothrow_awaitable), timeout_duration_);
  if (resolve_error)
    co_return resolve_error;
  co_return co_await establishNewConnection(resolve_result, io_context);
}

template<class SocketType>
asio::awaitable<std::error_code> ConnectionHandler<SocketType>::sendStreamWithDelimiter(const std::shared_ptr<io::InputStream>& stream_to_send,
    const std::vector<std::byte>& delimiter,
    asio::io_context& io_context) {
  if (auto connection_error = co_await setupUsableSocket(io_context))  // NOLINT
    co_return connection_error;
  co_return co_await send(stream_to_send, delimiter);
}

template<class SocketType>
asio::awaitable<std::error_code> ConnectionHandler<SocketType>::send(const std::shared_ptr<io::InputStream>& stream_to_send, const std::vector<std::byte>& delimiter) {
  gsl_Expects(hasUsableSocket());

  std::vector<std::byte> data_chunk;
  data_chunk.resize(chunk_size);
  gsl::span<std::byte> buffer{data_chunk};
  while (stream_to_send->tell() < stream_to_send->size()) {
    size_t num_read = stream_to_send->read(buffer);
    if (io::isError(num_read))
      co_return std::make_error_code(std::errc::io_error);
    auto [write_error, bytes_written] = co_await asyncOperationWithTimeout(asio::async_write(*socket_, asio::buffer(data_chunk, num_read), use_nothrow_awaitable), timeout_duration_);
    if (write_error)
      co_return write_error;
    logger_->log_trace("Writing flowfile(%zu bytes) to socket succeeded", bytes_written);
  }
  auto [delimiter_write_error, delimiter_bytes_written] = co_await asyncOperationWithTimeout(asio::async_write(*socket_, asio::buffer(delimiter), use_nothrow_awaitable), timeout_duration_);
  if (delimiter_write_error)
    co_return delimiter_write_error;
  logger_->log_trace("Writing delimiter(%zu bytes) to socket succeeded", delimiter_bytes_written);

  last_used_ = steady_clock::now();
  co_return std::error_code();
}
}  // namespace

void PutTCP::onTrigger(core::ProcessContext* context, core::ProcessSession* const session) {
  gsl_Expects(context && session);

  const auto flow_file = session->get();
  if (!flow_file) {
    yield();
    return;
  }

  removeExpiredConnections();

  auto hostname = context->getProperty(Hostname, flow_file).value_or(std::string{});
  auto port = context->getProperty(Port, flow_file).value_or(std::string{});
  if (hostname.empty() || port.empty()) {
    logger_->log_error("[%s] invalid target endpoint: hostname: %s, port: %s", flow_file->getUUIDStr(),
        hostname.empty() ? "(empty)" : hostname.c_str(),
        port.empty() ? "(empty)" : port.c_str());
    session->transfer(flow_file, Failure);
    return;
  }

  auto connection_id = detail::ConnectionId(std::move(hostname), std::move(port));
  std::shared_ptr<ConnectionHandlerBase> handler;
  if (!connections_ || !connections_->contains(connection_id)) {
    if (ssl_context_)
      handler = std::make_shared<ConnectionHandler<SslSocket>>(connection_id, timeout_duration_, logger_, max_size_of_socket_send_buffer_, &*ssl_context_);
    else
      handler = std::make_shared<ConnectionHandler<TcpSocket>>(connection_id, timeout_duration_, logger_, max_size_of_socket_send_buffer_, nullptr);
    if (connections_)
      (*connections_)[connection_id] = handler;
  } else {
    handler = (*connections_)[connection_id];
  }

  gsl_Expects(handler);

  processFlowFile(handler, *session, flow_file);
}

void PutTCP::removeExpiredConnections() {
  if (connections_) {
    std::erase_if(*connections_, [this](auto& item) -> bool {
      const auto& connection_handler = item.second;
      return (!connection_handler || (idle_connection_expiration_ && !connection_handler->hasBeenUsedIn(*idle_connection_expiration_)));
    });
  }
}

std::error_code PutTCP::sendFlowFileContent(std::shared_ptr<ConnectionHandlerBase>& connection_handler,
    const std::shared_ptr<io::InputStream>& flow_file_content_stream) {
  std::error_code operation_error;
  io_context_.restart();
  asio::co_spawn(io_context_,
      connection_handler->sendStreamWithDelimiter(flow_file_content_stream, delimiter_, io_context_),
      [&operation_error](const std::exception_ptr&, std::error_code error_code) {
        operation_error = error_code;
      });
  io_context_.run();
  return operation_error;
}

void PutTCP::processFlowFile(std::shared_ptr<ConnectionHandlerBase>& connection_handler,
    core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file) {
  auto flow_file_content_stream = session.getFlowFileContentStream(flow_file);
  if (!flow_file_content_stream) {
    session.transfer(flow_file, Failure);
    return;
  }

  std::error_code operation_error = sendFlowFileContent(connection_handler, flow_file_content_stream);

  if (operation_error && connection_handler->hasBeenUsed()) {
    logger_->log_warn("%s with reused connection, retrying...", operation_error.message());
    connection_handler->reset();
    operation_error = sendFlowFileContent(connection_handler, flow_file_content_stream);
  }

  if (operation_error) {
    connection_handler->reset();
    logger_->log_error("%s", operation_error.message());
    session.transfer(flow_file, Failure);
  } else {
    session.transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(PutTCP, Processor);

}  // namespace org::apache::nifi::minifi::processors
