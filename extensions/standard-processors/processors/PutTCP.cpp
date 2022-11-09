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

#include <algorithm>
#include <utility>

#include "range/v3/range/conversion.hpp"

#include "utils/gsl.h"
#include "utils/expected.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "core/logging/Logger.h"
#include "controllers/SSLContextService.h"

#include "asio/ssl.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/write.hpp"
#include "asio/high_resolution_timer.hpp"

using asio::ip::tcp;
using TcpSocket = asio::ip::tcp::socket;
using SslSocket = asio::ssl::stream<tcp::socket>;

using namespace std::literals::chrono_literals;

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
    timeout_ = timeout->getMilliseconds();
  else
    timeout_ = 15s;

  std::string context_name;
  ssl_context_service_.reset();
  if (context->getProperty(SSLContextService.getName(), context_name) && !IsNullOrEmpty(context_name)) {
    if (auto controller_service = context->getControllerService(context_name)) {
      ssl_context_service_ = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context->getControllerService(context_name));
      if (!ssl_context_service_)
        logger_->log_error("%s is not a SSL Context Service", context_name);
    } else {
      logger_->log_error("Invalid controller service: %s", context_name);
    }
  }

  delimiter_ = utils::span_to<std::vector>(gsl::make_span(context->getProperty(OutgoingMessageDelimiter).value_or(std::string{})).as_span<const std::byte>());

  if (context->getProperty<bool>(ConnectionPerFlowFile).value_or(false))
    connections_.reset();
  else
    connections_.emplace();

  if (auto max_size_of_socket_send_buffer = context->getProperty<core::DataSizeValue>(MaxSizeOfSocketSendBuffer))
    max_size_of_socket_send_buffer_ = max_size_of_socket_send_buffer->getValue();
  else
    max_size_of_socket_send_buffer_.reset();
}

namespace {
template<class SocketType>
class ConnectionHandler : public ConnectionHandlerBase {
 public:
  ConnectionHandler(detail::ConnectionId connection_id,
                    std::chrono::milliseconds timeout,
                    std::shared_ptr<core::logging::Logger> logger,
                    std::optional<size_t> max_size_of_socket_send_buffer,
                    std::shared_ptr<controllers::SSLContextService> ssl_context_service)
      : connection_id_(std::move(connection_id)),
        timeout_(timeout),
        logger_(std::move(logger)),
        max_size_of_socket_send_buffer_(max_size_of_socket_send_buffer),
        ssl_context_service_(std::move(ssl_context_service)) {
  }

  ~ConnectionHandler() override = default;

  nonstd::expected<void, std::error_code> sendData(const std::shared_ptr<io::InputStream>& flow_file_content_stream, const std::vector<std::byte>& delimiter) override;

 private:
  nonstd::expected<std::shared_ptr<SocketType>, std::error_code> getSocket();

  [[nodiscard]] bool hasBeenUsedIn(std::chrono::milliseconds dur) const override {
    return last_used_ && *last_used_ >= (std::chrono::steady_clock::now() - dur);
  }

  void reset() override {
    last_used_.reset();
    socket_.reset();
    io_context_.reset();
    last_error_.clear();
    deadline_.expires_at(asio::steady_timer::time_point::max());
  }

  void checkDeadline(std::error_code error_code, SocketType* socket);
  void startConnect(tcp::resolver::results_type::iterator endpoint_iter, const std::shared_ptr<SocketType>& socket);

  void handleConnect(std::error_code error,
                     tcp::resolver::results_type::iterator endpoint_iter,
                     const std::shared_ptr<SocketType>& socket);
  void handleConnectionSuccess(const tcp::resolver::results_type::iterator& endpoint_iter,
                               const std::shared_ptr<SocketType>& socket);
  void handleHandshake(std::error_code error,
                       const tcp::resolver::results_type::iterator& endpoint_iter,
                       const std::shared_ptr<SocketType>& socket);

  void handleWrite(std::error_code error,
                   std::size_t bytes_written,
                   const std::shared_ptr<io::InputStream>& flow_file_content_stream,
                   const std::vector<std::byte>& delimiter,
                   const std::shared_ptr<SocketType>& socket);

  void handleDelimiterWrite(std::error_code error, std::size_t bytes_written, const std::shared_ptr<SocketType>& socket);

  nonstd::expected<std::shared_ptr<SocketType>, std::error_code> establishConnection(const tcp::resolver::results_type& resolved_query);

  [[nodiscard]] bool hasBeenUsed() const override { return last_used_.has_value(); }

  detail::ConnectionId connection_id_;
  std::optional<std::chrono::steady_clock::time_point> last_used_;
  asio::io_context io_context_;
  std::error_code last_error_;
  asio::steady_timer deadline_{io_context_};
  std::chrono::milliseconds timeout_;
  std::shared_ptr<SocketType> socket_;

  std::shared_ptr<core::logging::Logger> logger_;
  std::optional<size_t> max_size_of_socket_send_buffer_;

  std::shared_ptr<controllers::SSLContextService> ssl_context_service_;

  nonstd::expected<tcp::resolver::results_type, std::error_code> resolveHostname();
  nonstd::expected<void, std::error_code> sendDataToSocket(const std::shared_ptr<SocketType>& socket,
                                                           const std::shared_ptr<io::InputStream>& flow_file_content_stream,
                                                           const std::vector<std::byte>& delimiter);
};

template<class SocketType>
nonstd::expected<void, std::error_code> ConnectionHandler<SocketType>::sendData(const std::shared_ptr<io::InputStream>& flow_file_content_stream, const std::vector<std::byte>& delimiter) {
  return getSocket() | utils::flatMap([&](const std::shared_ptr<SocketType>& socket) { return sendDataToSocket(socket, flow_file_content_stream, delimiter); });;
}

template<class SocketType>
nonstd::expected<std::shared_ptr<SocketType>, std::error_code> ConnectionHandler<SocketType>::getSocket() {
  if (socket_ && socket_->lowest_layer().is_open())
    return socket_;
  auto new_socket = resolveHostname() | utils::flatMap([&](const auto& resolved_query) { return establishConnection(resolved_query); });
  if (!new_socket)
    return nonstd::make_unexpected(new_socket.error());
  socket_ = std::move(*new_socket);
  return socket_;
}

template<class SocketType>
void ConnectionHandler<SocketType>::checkDeadline(std::error_code error_code, SocketType* socket) {
  if (error_code != asio::error::operation_aborted) {
    deadline_.expires_at(asio::steady_timer::time_point::max());
    last_error_ = asio::error::timed_out;
    deadline_.async_wait([&](std::error_code error_code) { checkDeadline(error_code, socket); });
    socket->lowest_layer().close();
  }
}

template<class SocketType>
void ConnectionHandler<SocketType>::startConnect(tcp::resolver::results_type::iterator endpoint_iter, const std::shared_ptr<SocketType>& socket) {
  if (endpoint_iter == tcp::resolver::results_type::iterator()) {
    logger_->log_trace("No more endpoints to try");
    deadline_.cancel();
    return;
  }

  last_error_.clear();
  deadline_.expires_after(timeout_);
  deadline_.async_wait([&](std::error_code error_code) -> void {
    checkDeadline(error_code, socket.get());
  });
  socket->lowest_layer().async_connect(endpoint_iter->endpoint(),
      [&socket, endpoint_iter, this](std::error_code err) {
        handleConnect(err, endpoint_iter, socket);
      });
}

template<class SocketType>
void ConnectionHandler<SocketType>::handleConnect(std::error_code error,
                                                  tcp::resolver::results_type::iterator endpoint_iter,
                                                  const std::shared_ptr<SocketType>& socket) {
  bool connection_failed_before_deadline = error.operator bool();
  bool connection_failed_due_to_deadline = !socket->lowest_layer().is_open();

  if (connection_failed_due_to_deadline) {
    core::logging::LOG_TRACE(logger_) << "Connecting to " << endpoint_iter->endpoint() << " timed out";
    socket->lowest_layer().close();
    return startConnect(++endpoint_iter, socket);
  }

  if (connection_failed_before_deadline) {
    core::logging::LOG_TRACE(logger_) << "Connecting to " << endpoint_iter->endpoint() << " failed due to " << error.message();
    last_error_ = error;
    socket->lowest_layer().close();
    return startConnect(++endpoint_iter, socket);
  }

  if (max_size_of_socket_send_buffer_)
    socket->lowest_layer().set_option(TcpSocket::send_buffer_size(*max_size_of_socket_send_buffer_));

  handleConnectionSuccess(endpoint_iter, socket);
}

template<class SocketType>
void ConnectionHandler<SocketType>::handleHandshake(std::error_code,
                                                    const tcp::resolver::results_type::iterator&,
                                                    const std::shared_ptr<SocketType>&) {
  throw std::invalid_argument("Handshake called without SSL");
}

template<>
void ConnectionHandler<SslSocket>::handleHandshake(std::error_code error,
                                                   const tcp::resolver::results_type::iterator& endpoint_iter,
                                                   const std::shared_ptr<SslSocket>& socket) {
  if (!error) {
    core::logging::LOG_TRACE(logger_) << "Successful handshake with " << endpoint_iter->endpoint();
    deadline_.cancel();
    return;
  }
  core::logging::LOG_TRACE(logger_) << "Handshake with " << endpoint_iter->endpoint() << " failed due to " << error.message();
  last_error_ = error;
  socket->lowest_layer().close();
  startConnect(std::next(endpoint_iter), socket);
}

template<>
void ConnectionHandler<TcpSocket>::handleConnectionSuccess(const tcp::resolver::results_type::iterator& endpoint_iter,
                                                           const std::shared_ptr<TcpSocket>& socket) {
  core::logging::LOG_TRACE(logger_) << "Connected to " << endpoint_iter->endpoint();
  socket->lowest_layer().non_blocking(true);
  deadline_.cancel();
}

template<>
void ConnectionHandler<SslSocket>::handleConnectionSuccess(const tcp::resolver::results_type::iterator& endpoint_iter,
                                                           const std::shared_ptr<SslSocket>& socket) {
  core::logging::LOG_TRACE(logger_) << "Connected to " << endpoint_iter->endpoint();
  socket->async_handshake(asio::ssl::stream_base::client, [this, &socket, endpoint_iter](const std::error_code handshake_error) {
    handleHandshake(handshake_error, endpoint_iter, socket);
  });
}

template<class SocketType>
void ConnectionHandler<SocketType>::handleWrite(std::error_code error,
                                                std::size_t bytes_written,
                                                const std::shared_ptr<io::InputStream>& flow_file_content_stream,
                                                const std::vector<std::byte>& delimiter,
                                                const std::shared_ptr<SocketType>& socket) {
  bool write_failed_before_deadline = error.operator bool();
  bool write_failed_due_to_deadline = !socket->lowest_layer().is_open();

  if (write_failed_due_to_deadline) {
    logger_->log_trace("Writing flowfile to socket timed out");
    socket->lowest_layer().close();
    deadline_.cancel();
    return;
  }

  if (write_failed_before_deadline) {
    last_error_ = error;
    logger_->log_trace("Writing flowfile to socket failed due to %s", error.message());
    socket->lowest_layer().close();
    deadline_.cancel();
    return;
  }

  logger_->log_trace("Writing flowfile(%zu bytes) to socket succeeded", bytes_written);
  if (flow_file_content_stream->size() == flow_file_content_stream->tell()) {
    asio::async_write(*socket, asio::buffer(delimiter), [&](std::error_code error, std::size_t bytes_written) {
      handleDelimiterWrite(error, bytes_written, socket);
    });
  } else {
    std::vector<std::byte> data_chunk;
    data_chunk.resize(chunk_size);
    gsl::span<std::byte> buffer{data_chunk};
    size_t num_read = flow_file_content_stream->read(buffer);
    asio::async_write(*socket, asio::buffer(data_chunk, num_read), [&](const std::error_code err, std::size_t bytes_written) {
      handleWrite(err, bytes_written, flow_file_content_stream, delimiter, socket);
    });
  }
}

template<class SocketType>
void ConnectionHandler<SocketType>::handleDelimiterWrite(std::error_code error, std::size_t bytes_written, const std::shared_ptr<SocketType>& socket) {
  bool write_failed_before_deadline = error.operator bool();
  bool write_failed_due_to_deadline = !socket->lowest_layer().is_open();

  if (write_failed_due_to_deadline) {
    logger_->log_trace("Writing delimiter to socket timed out");
    socket->lowest_layer().close();
    deadline_.cancel();
    return;
  }

  if (write_failed_before_deadline) {
    last_error_ = error;
    logger_->log_trace("Writing delimiter to socket failed due to %s", error.message());
    socket->lowest_layer().close();
    deadline_.cancel();
    return;
  }

  logger_->log_trace("Writing delimiter(%zu bytes) to socket succeeded", bytes_written);
  deadline_.cancel();
}


template<>
nonstd::expected<std::shared_ptr<TcpSocket>, std::error_code> ConnectionHandler<TcpSocket>::establishConnection(const tcp::resolver::results_type& resolved_query) {
  auto socket = std::make_shared<TcpSocket>(io_context_);
  startConnect(resolved_query.begin(), socket);
  deadline_.expires_after(timeout_);
  deadline_.async_wait([&](std::error_code error_code) -> void {
    checkDeadline(error_code, socket.get());
  });
  io_context_.run();
  if (last_error_)
    return nonstd::make_unexpected(last_error_);
  return socket;
}

asio::ssl::context getSslContext(const auto& ssl_context_service) {
  gsl_Expects(ssl_context_service);
  asio::ssl::context ssl_context(asio::ssl::context::sslv23);
  ssl_context.load_verify_file(ssl_context_service->getCACertificate());
  ssl_context.set_verify_mode(asio::ssl::verify_peer);
  if (auto cert_file = ssl_context_service->getCertificateFile(); !cert_file.empty())
    ssl_context.use_certificate_file(cert_file, asio::ssl::context::pem);
  if (auto private_key_file = ssl_context_service->getPrivateKeyFile(); !private_key_file.empty())
    ssl_context.use_private_key_file(private_key_file, asio::ssl::context::pem);
  ssl_context.set_password_callback([password = ssl_context_service->getPassphrase()](std::size_t&, asio::ssl::context_base::password_purpose&) { return password; });
  return ssl_context;
}

template<>
nonstd::expected<std::shared_ptr<SslSocket>, std::error_code> ConnectionHandler<SslSocket>::establishConnection(const tcp::resolver::results_type& resolved_query) {
  auto ssl_context = getSslContext(ssl_context_service_);
  auto socket = std::make_shared<SslSocket>(io_context_, ssl_context);
  startConnect(resolved_query.begin(), socket);
  deadline_.async_wait([&](std::error_code error_code) -> void {
    checkDeadline(error_code, socket.get());
  });
  io_context_.run();
  if (last_error_)
    return nonstd::make_unexpected(last_error_);
  return socket;
}

template<class SocketType>
nonstd::expected<void, std::error_code> ConnectionHandler<SocketType>::sendDataToSocket(const std::shared_ptr<SocketType>& socket,
                                                                                        const std::shared_ptr<io::InputStream>& flow_file_content_stream,
                                                                                        const std::vector<std::byte>& delimiter) {
  if (!socket || !socket->lowest_layer().is_open())
    return nonstd::make_unexpected(asio::error::not_socket);

  deadline_.expires_after(timeout_);
  deadline_.async_wait([&](std::error_code error_code) -> void {
    checkDeadline(error_code, socket.get());
  });
  io_context_.restart();

  std::vector<std::byte> data_chunk;
  data_chunk.resize(chunk_size);

  gsl::span<std::byte> buffer{data_chunk};
  size_t num_read = flow_file_content_stream->read(buffer);
  logger_->log_trace("read %zu bytes from flowfile", num_read);
  asio::async_write(*socket, asio::buffer(data_chunk, num_read), [&](const std::error_code err, std::size_t bytes_written) {
    handleWrite(err, bytes_written, flow_file_content_stream, delimiter, socket);
  });
  deadline_.async_wait([&](std::error_code error_code) -> void {
    checkDeadline(error_code, socket.get());
  });
  io_context_.run();
  if (last_error_)
    return nonstd::make_unexpected(last_error_);
  last_used_ = std::chrono::steady_clock::now();
  return {};
}

template<class SocketType>
nonstd::expected<tcp::resolver::results_type, std::error_code> ConnectionHandler<SocketType>::resolveHostname() {
  tcp::resolver resolver(io_context_);
  std::error_code error_code;
  auto resolved_query = resolver.resolve(connection_id_.getHostname(), connection_id_.getPort(), error_code);
  if (error_code)
    return nonstd::make_unexpected(error_code);
  return resolved_query;
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

  auto flow_file_content_stream = session->getFlowFileContentStream(flow_file);
  if (!flow_file_content_stream) {
    session->transfer(flow_file, Failure);
    return;
  }

  auto connection_id = detail::ConnectionId(std::move(hostname), std::move(port));
  std::shared_ptr<ConnectionHandlerBase> handler;
  if (!connections_ || !connections_->contains(connection_id)) {
    if (ssl_context_service_)
      handler = std::make_shared<ConnectionHandler<SslSocket>>(connection_id, timeout_, logger_, max_size_of_socket_send_buffer_, ssl_context_service_);
    else
      handler = std::make_shared<ConnectionHandler<TcpSocket>>(connection_id, timeout_, logger_, max_size_of_socket_send_buffer_, nullptr);
    if (connections_)
      (*connections_)[connection_id] = handler;
  } else {
    handler = (*connections_)[connection_id];
  }

  gsl_Expects(handler);

  processFlowFile(handler, flow_file_content_stream, *session, flow_file);
}

void PutTCP::removeExpiredConnections() {
  if (connections_) {
    std::erase_if(*connections_, [this](auto& item) -> bool {
      const auto& connection_handler = item.second;
      return (!connection_handler || (idle_connection_expiration_ && !connection_handler->hasBeenUsedIn(*idle_connection_expiration_)));
    });
  }
}

void PutTCP::processFlowFile(std::shared_ptr<ConnectionHandlerBase>& connection_handler,
                             const std::shared_ptr<io::InputStream>& flow_file_content_stream,
                             core::ProcessSession& session,
                             const std::shared_ptr<core::FlowFile>& flow_file) {
  auto result = connection_handler->sendData(flow_file_content_stream, delimiter_);

  if (!result && connection_handler->hasBeenUsed()) {
    logger_->log_warn("%s with reused connection, retrying...", result.error().message());
    connection_handler->reset();
    result = connection_handler->sendData(flow_file_content_stream, delimiter_);
  }

  const auto transfer_to_success = [&session, &flow_file]() -> void {
    session.transfer(flow_file, Success);
  };

  const auto transfer_to_failure = [&session, &flow_file, &logger = logger_, &connection_handler](std::error_code ec) -> void {
    gsl_Expects(ec);
    connection_handler->reset();
    logger->log_error("%s", ec.message());
    session.transfer(flow_file, Failure);
  };

  result | utils::map(transfer_to_success) | utils::orElse(transfer_to_failure);
}

REGISTER_RESOURCE(PutTCP, Processor);

}  // namespace org::apache::nifi::minifi::processors
