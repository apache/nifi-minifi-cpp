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
#ifndef CONTROLLER_CONTROLLER_H_
#define CONTROLLER_CONTROLLER_H_

#include "io/ClientSocket.h"
#include "c2/ControllerSocketProtocol.h"

/**
 * Sends a single argument comment
 * @param socket socket unique ptr.
 * @param op operation to perform
 * @param value value to send
 */
bool sendSingleCommand(std::unique_ptr<minifi::io::Socket> socket, uint8_t op, const std::string value) {
  socket->initialize();
  std::vector<uint8_t> data;
  minifi::io::BaseStream stream;
  stream.writeData(&op, 1);
  stream.writeUTF(value);
  socket->writeData(const_cast<uint8_t*>(stream.getBuffer()), stream.getSize());
  return true;
}

/**
 * Stops a stopped component
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool stopComponent(std::unique_ptr<minifi::io::Socket> socket, std::string component) {
  return sendSingleCommand(std::move(socket), minifi::c2::Operation::STOP, component);
}

/**
 * Starts a previously stopped component.
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool startComponent(std::unique_ptr<minifi::io::Socket> socket, std::string component) {
  return sendSingleCommand(std::move(socket), minifi::c2::Operation::START, component);
}

/**
 * Clears a connection queue.
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool clearConnection(std::unique_ptr<minifi::io::Socket> socket, std::string connection) {
  return sendSingleCommand(std::move(socket), minifi::c2::Operation::CLEAR, connection);
}

/**
 * Updates the flow to the provided file
 */
void updateFlow(std::unique_ptr<minifi::io::Socket> socket, std::ostream &out, std::string file) {
  socket->initialize();
  std::vector<uint8_t> data;
  uint8_t op = minifi::c2::Operation::UPDATE;
  minifi::io::BaseStream stream;
  stream.writeData(&op, 1);
  stream.writeUTF("flow");
  stream.writeUTF(file);
  socket->writeData(const_cast<uint8_t*>(stream.getBuffer()), stream.getSize());

  // read the response
  uint8_t resp = 0;
  socket->readData(&resp, 1);
  if (resp == minifi::c2::Operation::DESCRIBE) {
    uint16_t connections = 0;
    socket->read(connections);
    out << connections << " are full" << std::endl;
    for (int i = 0; i < connections; i++) {
      std::string fullcomponent;
      socket->readUTF(fullcomponent);
      out << fullcomponent << " is full" << std::endl;
    }
  }
}

/**
 * Lists connections which are full
 * @param socket socket ptr
 */
void getFullConnections(std::unique_ptr<minifi::io::Socket> socket, std::ostream &out) {
  socket->initialize();
  std::vector<uint8_t> data;
  uint8_t op = minifi::c2::Operation::DESCRIBE;
  minifi::io::BaseStream stream;
  stream.writeData(&op, 1);
  stream.writeUTF("getfull");
  socket->writeData(const_cast<uint8_t*>(stream.getBuffer()), stream.getSize());

  // read the response
  uint8_t resp = 0;
  socket->readData(&resp, 1);
  if (resp == minifi::c2::Operation::DESCRIBE) {
    uint16_t connections = 0;
    socket->read(connections);
    out << connections << " are full" << std::endl;
    for (int i = 0; i < connections; i++) {
      std::string fullcomponent;
      socket->readUTF(fullcomponent);
      out << fullcomponent << " is full" << std::endl;
    }

  }
}

/**
 * Prints the connection size for the provided connection.
 * @param socket socket ptr
 * @param connection connection whose size will be returned.
 */
void getConnectionSize(std::unique_ptr<minifi::io::Socket> socket, std::ostream &out, std::string connection) {
  socket->initialize();
  std::vector<uint8_t> data;
  uint8_t op = minifi::c2::Operation::DESCRIBE;
  minifi::io::BaseStream stream;
  stream.writeData(&op, 1);
  stream.writeUTF("queue");
  stream.writeUTF(connection);
  socket->writeData(const_cast<uint8_t*>(stream.getBuffer()), stream.getSize());

  // read the response
  uint8_t resp = 0;
  socket->readData(&resp, 1);
  if (resp == minifi::c2::Operation::DESCRIBE) {
    std::string size;
    socket->readUTF(size);
    out << "Size/Max of " << connection << " " << size << std::endl;
  }
}

void listProcessors(std::unique_ptr<minifi::io::Socket> socket, std::ostream &out) {
  socket->initialize();
  minifi::io::BaseStream stream;
  uint8_t op = minifi::c2::Operation::DESCRIBE;
  stream.writeData(&op, 1);
  stream.writeUTF("processors");
  socket->writeData(const_cast<uint8_t*>(stream.getBuffer()), stream.getSize());
  uint16_t responses = 0;
  socket->readData(&op, 1);
  socket->read(responses);
  out << "Processors:" << std::endl;

  for (int i = 0; i < responses; i++) {
    std::string name;
    socket->readUTF(name, false);
    out << name << std::endl;
  }
}

void listConnections(std::unique_ptr<minifi::io::Socket> socket, std::ostream &out) {
  socket->initialize();
  minifi::io::BaseStream stream;
  uint8_t op = minifi::c2::Operation::DESCRIBE;
  stream.writeData(&op, 1);
  stream.writeUTF("connections");
  socket->writeData(const_cast<uint8_t*>(stream.getBuffer()), stream.getSize());
  uint16_t responses = 0;
  socket->readData(&op, 1);
  socket->read(responses);

  out << "Connection Names:" << std::endl;

  for (int i = 0; i < responses; i++) {
    std::string name;
    socket->readUTF(name, false);
    out << name << std::endl;
  }
}

#endif /* CONTROLLER_CONTROLLER_H_ */
