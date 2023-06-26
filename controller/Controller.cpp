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
#include "Controller.h"

#include <utility>

#include "io/BufferStream.h"
#include "c2/C2Payload.h"
#include "io/AsioStream.h"
#include "asio/ssl/context.hpp"
#include "asio/ssl/stream.hpp"
#include "asio/connect.hpp"
#include "core/logging/Logger.h"
#include "utils/net/AsioSocketUtils.h"

namespace org::apache::nifi::minifi::controller {

namespace {

class ClientConnection {
 public:
  explicit ClientConnection(const ControllerSocketData& socket_data) {
    if (socket_data.ssl_context_service) {
      connectTcpSocketOverSsl(socket_data);
    } else {
      connectTcpSocket(socket_data);
    }
  }

  [[nodiscard]] io::BaseStream* getStream() const {
    return stream_.get();
  }

 private:
  void connectTcpSocketOverSsl(const ControllerSocketData& socket_data) {
    auto ssl_context = utils::net::getSslContext(*socket_data.ssl_context_service);
    asio::ssl::stream<asio::ip::tcp::socket> socket(io_context_, ssl_context);

    asio::ip::tcp::resolver resolver(io_context_);
    asio::error_code err;
    asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(socket_data.host, std::to_string(socket_data.port), err);
    if (err) {
      logger_->log_error("Resolving host '%s' on port '%s' failed with the following message: '%s'", socket_data.host, std::to_string(socket_data.port), err.message());
      return;
    }

    asio::connect(socket.lowest_layer(), endpoints, err);
    if (err) {
      logger_->log_error("Connecting to host '%s' on port '%s' failed with the following message: '%s'", socket_data.host, std::to_string(socket_data.port), err.message());
      return;
    }
    socket.handshake(asio::ssl::stream_base::client, err);
    if (err) {
      logger_->log_error("SSL handshake failed while connecting to host '%s' on port '%s' with the following message: '%s'", socket_data.host, std::to_string(socket_data.port), err.message());
      return;
    }
    stream_ = std::make_unique<io::AsioStream<asio::ssl::stream<asio::ip::tcp::socket>>>(std::move(socket));
  }

  void connectTcpSocket(const ControllerSocketData& socket_data) {
    asio::ip::tcp::socket socket(io_context_);

    asio::ip::tcp::resolver resolver(io_context_);
    asio::error_code err;
    asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(socket_data.host, std::to_string(socket_data.port));
    if (err) {
      logger_->log_error("Resolving host '%s' on port '%s' failed with the following message: '%s'", socket_data.host, std::to_string(socket_data.port), err.message());
      return;
    }

    asio::connect(socket, endpoints, err);
    if (err) {
      logger_->log_error("Connecting to host '%s' on port '%s' failed with the following message: '%s'", socket_data.host, std::to_string(socket_data.port), err.message());
      return;
    }
    stream_ = std::make_unique<io::AsioStream<asio::ip::tcp::socket>>(std::move(socket));
  }

  asio::io_context io_context_;
  std::unique_ptr<io::BaseStream> stream_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<ClientConnection>::getLogger()};
};

}  // namespace


bool sendSingleCommand(const ControllerSocketData& socket_data, uint8_t op, const std::string& value) {
  ClientConnection connection(socket_data);
  auto connection_stream = connection.getStream();
  if (!connection_stream) {
    return false;
  }
  io::BufferStream buffer;
  buffer.write(&op, 1);
  buffer.write(value);
  return connection_stream->write(buffer.getBuffer()) == buffer.size();
}

bool stopComponent(const ControllerSocketData& socket_data, const std::string& component) {
  return sendSingleCommand(socket_data, static_cast<uint8_t>(c2::Operation::stop), component);
}

bool startComponent(const ControllerSocketData& socket_data, const std::string& component) {
  return sendSingleCommand(socket_data, static_cast<uint8_t>(c2::Operation::start), component);
}

bool clearConnection(const ControllerSocketData& socket_data, const std::string& connection) {
  return sendSingleCommand(socket_data, static_cast<uint8_t>(c2::Operation::clear), connection);
}

bool updateFlow(const ControllerSocketData& socket_data, std::ostream &out, const std::string& file) {
  ClientConnection connection(socket_data);
  auto connection_stream = connection.getStream();
  if (!connection_stream) {
    return false;
  }
  auto op = static_cast<uint8_t>(c2::Operation::update);
  io::BufferStream buffer;
  buffer.write(&op, 1);
  buffer.write("flow");
  buffer.write(file);
  if (io::isError(connection_stream->write(buffer.getBuffer()))) {
    return false;
  }
  // read the response
  uint8_t resp = 0;
  connection_stream->read(resp);
  if (resp == static_cast<uint8_t>(c2::Operation::describe)) {
    uint16_t connections = 0;
    connection_stream->read(connections);
    out << connections << " are full" << std::endl;
    for (int i = 0; i < connections; i++) {
      std::string fullcomponent;
      connection_stream->read(fullcomponent);
      out << fullcomponent << " is full" << std::endl;
    }
  }
  return true;
}

bool getFullConnections(const ControllerSocketData& socket_data, std::ostream &out) {
  ClientConnection connection(socket_data);
  auto connection_stream = connection.getStream();
  if (!connection_stream) {
    return false;
  }
  auto op = static_cast<uint8_t>(c2::Operation::describe);
  io::BufferStream buffer;
  buffer.write(&op, 1);
  buffer.write("getfull");
  if (io::isError(connection_stream->write(buffer.getBuffer()))) {
    return false;
  }
  // read the response
  uint8_t resp = 0;
  connection_stream->read(resp);
  if (resp == static_cast<uint8_t>(c2::Operation::describe)) {
    uint16_t connections = 0;
    connection_stream->read(connections);
    out << connections << " are full" << std::endl;
    for (int i = 0; i < connections; i++) {
      std::string fullcomponent;
      connection_stream->read(fullcomponent);
      out << fullcomponent << " is full" << std::endl;
    }
  }
  return true;
}

bool getConnectionSize(const ControllerSocketData& socket_data, std::ostream &out, const std::string& connection) {
  ClientConnection client_connection(socket_data);
  auto connection_stream = client_connection.getStream();
  if (!connection_stream) {
    return false;
  }
  auto op = static_cast<uint8_t>(c2::Operation::describe);
  io::BufferStream buffer;
  buffer.write(&op, 1);
  buffer.write("queue");
  buffer.write(connection);
  if (io::isError(connection_stream->write(buffer.getBuffer()))) {
    return false;
  }
  // read the response
  uint8_t resp = 0;
  connection_stream->read(resp);
  if (resp == static_cast<uint8_t>(c2::Operation::describe)) {
    std::string size;
    connection_stream->read(size);
    out << "Size/Max of " << connection << " " << size << std::endl;
  }
  return true;
}

bool listComponents(const ControllerSocketData& socket_data, std::ostream &out, bool show_header) {
  ClientConnection connection(socket_data);
  auto connection_stream = connection.getStream();
  if (!connection_stream) {
    return false;
  }
  io::BufferStream buffer;
  auto op = static_cast<uint8_t>(c2::Operation::describe);
  buffer.write(&op, 1);
  buffer.write("components");
  if (io::isError(connection_stream->write(buffer.getBuffer()))) {
    return false;
  }
  uint16_t responses = 0;
  connection_stream->read(op);
  connection_stream->read(responses);
  if (show_header)
    out << "Components:" << std::endl;

  for (int i = 0; i < responses; i++) {
    std::string name;
    connection_stream->read(name, false);
    std::string status;
    connection_stream->read(status, false);
    out << name << ", running: " << status << std::endl;
  }
  return true;
}

bool listConnections(const ControllerSocketData& socket_data, std::ostream &out, bool show_header) {
  ClientConnection connection(socket_data);
  auto connection_stream = connection.getStream();
  if (!connection_stream) {
    return false;
  }
  io::BufferStream buffer;
  auto op = static_cast<uint8_t>(c2::Operation::describe);
  buffer.write(&op, 1);
  buffer.write("connections");
  if (io::isError(connection_stream->write(buffer.getBuffer()))) {
    return false;
  }
  uint16_t responses = 0;
  connection_stream->read(op);
  connection_stream->read(responses);
  if (show_header)
    out << "Connection Names:" << std::endl;

  for (int i = 0; i < responses; i++) {
    std::string name;
    connection_stream->read(name, false);
    out << name << std::endl;
  }
  return true;
}

bool printManifest(const ControllerSocketData& socket_data, std::ostream &out) {
  ClientConnection connection(socket_data);
  auto connection_stream = connection.getStream();
  if (!connection_stream) {
    return false;
  }
  io::BufferStream buffer;
  auto op = static_cast<uint8_t>(c2::Operation::describe);
  buffer.write(&op, 1);
  buffer.write("manifest");
  if (io::isError(connection_stream->write(buffer.getBuffer()))) {
    return false;
  }
  connection_stream->read(op);
  std::string manifest;
  connection_stream->read(manifest, true);
  out << manifest << std::endl;
  return true;
}

bool getJstacks(const ControllerSocketData& socket_data, std::ostream &out) {
  ClientConnection connection(socket_data);
  auto connection_stream = connection.getStream();
  if (!connection_stream) {
    return false;
  }
  io::BufferStream buffer;
  auto op = static_cast<uint8_t>(c2::Operation::describe);
  buffer.write(&op, 1);
  buffer.write("jstack");
  if (io::isError(connection_stream->write(buffer.getBuffer()))) {
    return false;
  }
  connection_stream->read(op);
  std::string manifest;
  connection_stream->read(manifest, true);
  out << manifest << std::endl;
  return true;
}

}  // namespace org::apache::nifi::minifi::controller
