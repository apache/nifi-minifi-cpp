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
#include <fstream>

#include "io/BufferStream.h"
#include "c2/C2Payload.h"
#include "io/AsioStream.h"
#include "asio/ssl/context.hpp"
#include "asio/ssl/stream.hpp"
#include "asio/connect.hpp"
#include "minifi-cpp/core/logging/Logger.h"
#include "utils/ConfigurationUtils.h"
#include "utils/net/AsioSocketUtils.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::controller {

bool sendSingleCommand(const utils::net::SocketData& socket_data, uint8_t op, const std::string& value) {
  const auto connection_stream = std::make_unique<utils::net::AsioSocketConnection>(socket_data);
  if (connection_stream->initialize() < 0) {
    return false;
  }
  io::BufferStream buffer;
  buffer.write(&op, 1);
  buffer.write(value);
  return connection_stream->write(buffer.getBuffer()) == buffer.size();
}

bool stopComponent(const utils::net::SocketData& socket_data, const std::string& component) {
  return sendSingleCommand(socket_data, static_cast<uint8_t>(c2::Operation::stop), component);
}

bool startComponent(const utils::net::SocketData& socket_data, const std::string& component) {
  return sendSingleCommand(socket_data, static_cast<uint8_t>(c2::Operation::start), component);
}

bool clearConnection(const utils::net::SocketData& socket_data, const std::string& connection) {
  return sendSingleCommand(socket_data, static_cast<uint8_t>(c2::Operation::clear), connection);
}

bool updateFlow(const utils::net::SocketData& socket_data, std::ostream &out, const std::string& file) {
  const auto connection_stream = std::make_unique<utils::net::AsioSocketConnection>(socket_data);
  if (connection_stream->initialize() < 0) {
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
    for (uint16_t i = 0; i < connections; i++) {
      std::string fullcomponent;
      connection_stream->read(fullcomponent);
      out << fullcomponent << " is full" << std::endl;
    }
  }
  return true;
}

bool getFullConnections(const utils::net::SocketData& socket_data, std::ostream &out) {
  const auto connection_stream = std::make_unique<utils::net::AsioSocketConnection>(socket_data);
  if (connection_stream->initialize() < 0) {
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
    for (uint16_t i = 0; i < connections; i++) {
      std::string fullcomponent;
      connection_stream->read(fullcomponent);
      out << fullcomponent << " is full" << std::endl;
    }
  }
  return true;
}

bool getConnectionSize(const utils::net::SocketData& socket_data, std::ostream &out, const std::string& connection) {
  const auto connection_stream = std::make_unique<utils::net::AsioSocketConnection>(socket_data);
  if (connection_stream->initialize() < 0) {
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

bool listComponents(const utils::net::SocketData& socket_data, std::ostream &out, bool show_header) {
  const auto connection_stream = std::make_unique<utils::net::AsioSocketConnection>(socket_data);
  if (connection_stream->initialize() < 0) {
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

  for (uint16_t i = 0; i < responses; i++) {
    std::string name;
    connection_stream->read(name, false);
    std::string status;
    connection_stream->read(status, false);
    out << name << ", running: " << status << std::endl;
  }
  return true;
}

bool listConnections(const utils::net::SocketData& socket_data, std::ostream &out, bool show_header) {
  const auto connection_stream = std::make_unique<utils::net::AsioSocketConnection>(socket_data);
  if (connection_stream->initialize() < 0) {
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

  for (uint16_t i = 0; i < responses; i++) {
    std::string name;
    connection_stream->read(name, false);
    out << name << std::endl;
  }
  return true;
}

bool printManifest(const utils::net::SocketData& socket_data, std::ostream &out) {
  const auto connection_stream = std::make_unique<utils::net::AsioSocketConnection>(socket_data);
  if (connection_stream->initialize() < 0) {
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

bool getJstacks(const utils::net::SocketData& socket_data, std::ostream &out) {
  const auto connection_stream = std::make_unique<utils::net::AsioSocketConnection>(socket_data);
  if (connection_stream->initialize() < 0) {
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

nonstd::expected<void, std::string> getDebugBundle(const utils::net::SocketData& socket_data, const std::filesystem::path& target_dir) {
  const auto connection_stream = std::make_unique<utils::net::AsioSocketConnection>(socket_data);
  if (connection_stream->initialize() < 0) {
    return nonstd::make_unexpected("Could not connect to remote host " + socket_data.host + ":" + std::to_string(socket_data.port));
  }
  io::BufferStream buffer;
  auto op = static_cast<uint8_t>(c2::Operation::transfer);
  buffer.write(&op, 1);
  buffer.write("debug");
  if (io::isError(connection_stream->write(buffer.getBuffer()))) {
    return nonstd::make_unexpected("Could not write to connection " + socket_data.host + ":" + std::to_string(socket_data.port));
  }
  connection_stream->read(op);
  size_t bundle_size = 0;
  connection_stream->read(bundle_size);
  if (bundle_size == 0) {
    return nonstd::make_unexpected("Failed to retrieve debug bundle");
  }

  if (std::filesystem::exists(target_dir) && !std::filesystem::is_directory(target_dir)) {
    return nonstd::make_unexpected("Object specified as the target directory already exists and it is not a directory");
  }

  if (!std::filesystem::exists(target_dir) && utils::file::create_dir(target_dir) != 0) {
    return nonstd::make_unexpected("Failed to create target directory: " + target_dir.string());
  }

  std::ofstream out_file(target_dir / "debug.tar.gz");
  static constexpr auto BUFFER_SIZE = utils::configuration::DEFAULT_BUFFER_SIZE;
  std::array<char, BUFFER_SIZE> out_buffer{};
  while (bundle_size > 0) {
    const auto next_read_size = (std::min)(bundle_size, BUFFER_SIZE);
    const auto size_read = connection_stream->read(std::as_writable_bytes(std::span(out_buffer).subspan(0, next_read_size)));
    bundle_size -= size_read;
    out_file.write(out_buffer.data(), gsl::narrow<std::streamsize>(size_read));
  }
  return {};
}

bool getFlowStatus(const utils::net::SocketData& socket_data, const std::string& status_query, std::ostream &out) {
  const auto connection_stream = std::make_unique<utils::net::AsioSocketConnection>(socket_data);
  if (connection_stream->initialize() < 0) {
    return false;
  }
  io::BufferStream buffer;
  auto op = static_cast<uint8_t>(c2::Operation::describe);
  buffer.write(&op, 1);
  buffer.write("flowstatus");
  buffer.write(status_query);
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
