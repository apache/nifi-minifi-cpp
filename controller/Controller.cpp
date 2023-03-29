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

namespace org::apache::nifi::minifi::controller {

bool sendSingleCommand(std::unique_ptr<io::Socket> socket, uint8_t op, const std::string& value) {
  if (socket->initialize() < 0) {
    return false;
  }
  io::BufferStream stream;
  stream.write(&op, 1);
  stream.write(value);
  return socket->write(stream.getBuffer()) == stream.size();
}

bool stopComponent(std::unique_ptr<io::Socket> socket, const std::string& component) {
  return sendSingleCommand(std::move(socket), c2::Operation::STOP, component);
}

bool startComponent(std::unique_ptr<io::Socket> socket, const std::string& component) {
  return sendSingleCommand(std::move(socket), c2::Operation::START, component);
}

bool clearConnection(std::unique_ptr<io::Socket> socket, const std::string& connection) {
  return sendSingleCommand(std::move(socket), c2::Operation::CLEAR, connection);
}

int updateFlow(std::unique_ptr<io::Socket> socket, std::ostream &out, const std::string& file) {
  if (socket->initialize() < 0) {
    return -1;
  }
  uint8_t op = c2::Operation::UPDATE;
  io::BufferStream stream;
  stream.write(&op, 1);
  stream.write("flow");
  stream.write(file);
  if (io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  // read the response
  uint8_t resp = 0;
  socket->read(resp);
  if (resp == c2::Operation::DESCRIBE) {
    uint16_t connections = 0;
    socket->read(connections);
    out << connections << " are full" << std::endl;
    for (int i = 0; i < connections; i++) {
      std::string fullcomponent;
      socket->read(fullcomponent);
      out << fullcomponent << " is full" << std::endl;
    }
  }
  return 0;
}

int getFullConnections(std::unique_ptr<io::Socket> socket, std::ostream &out) {
  if (socket->initialize() < 0) {
    return -1;
  }
  uint8_t op = c2::Operation::DESCRIBE;
  io::BufferStream stream;
  stream.write(&op, 1);
  stream.write("getfull");
  if (io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  // read the response
  uint8_t resp = 0;
  socket->read(resp);
  if (resp == c2::Operation::DESCRIBE) {
    uint16_t connections = 0;
    socket->read(connections);
    out << connections << " are full" << std::endl;
    for (int i = 0; i < connections; i++) {
      std::string fullcomponent;
      socket->read(fullcomponent);
      out << fullcomponent << " is full" << std::endl;
    }
  }
  return 0;
}

int getConnectionSize(std::unique_ptr<io::Socket> socket, std::ostream &out, const std::string& connection) {
  if (socket->initialize() < 0) {
    return -1;
  }
  uint8_t op = c2::Operation::DESCRIBE;
  io::BufferStream stream;
  stream.write(&op, 1);
  stream.write("queue");
  stream.write(connection);
  if (io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  // read the response
  uint8_t resp = 0;
  socket->read(resp);
  if (resp == c2::Operation::DESCRIBE) {
    std::string size;
    socket->read(size);
    out << "Size/Max of " << connection << " " << size << std::endl;
  }
  return 0;
}

int listComponents(std::unique_ptr<io::Socket> socket, std::ostream &out, bool show_header) {
  if (socket->initialize() < 0) {
    return -1;
  }
  io::BufferStream stream;
  uint8_t op = c2::Operation::DESCRIBE;
  stream.write(&op, 1);
  stream.write("components");
  if (io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  uint16_t responses = 0;
  socket->read(op);
  socket->read(responses);
  if (show_header)
    out << "Components:" << std::endl;

  for (int i = 0; i < responses; i++) {
    std::string name;
    socket->read(name, false);
    std::string status;
    socket->read(status, false);
    out << name << ", running: " << status << std::endl;
  }
  return 0;
}

int listConnections(std::unique_ptr<io::Socket> socket, std::ostream &out, bool show_header) {
  if (socket->initialize() < 0) {
    return -1;
  }
  io::BufferStream stream;
  uint8_t op = c2::Operation::DESCRIBE;
  stream.write(&op, 1);
  stream.write("connections");
  if (io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  uint16_t responses = 0;
  socket->read(op);
  socket->read(responses);
  if (show_header)
    out << "Connection Names:" << std::endl;

  for (int i = 0; i < responses; i++) {
    std::string name;
    socket->read(name, false);
    out << name << std::endl;
  }
  return 0;
}

int printManifest(std::unique_ptr<io::Socket> socket, std::ostream &out) {
  if (socket->initialize() < 0) {
    return -1;
  }
  io::BufferStream stream;
  uint8_t op = c2::Operation::DESCRIBE;
  stream.write(&op, 1);
  stream.write("manifest");
  if (io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  socket->read(op);
  std::string manifest;
  socket->read(manifest, true);
  out << manifest << std::endl;
  return 0;
}

int getJstacks(std::unique_ptr<io::Socket> socket, std::ostream &out) {
  if (socket->initialize() < 0) {
    return -1;
  }
  io::BufferStream stream;
  uint8_t op = c2::Operation::DESCRIBE;
  stream.write(&op, 1);
  stream.write("jstack");
  if (io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  socket->read(op);
  std::string manifest;
  socket->read(manifest, true);
  out << manifest << std::endl;
  return 0;
}

}  // namespace org::apache::nifi::minifi::controller
