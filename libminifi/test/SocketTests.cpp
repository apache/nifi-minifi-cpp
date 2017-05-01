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

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file

#include "TestBase.h"
#include "io/ClientSocket.h"

using namespace org::apache::nifi::minifi::io;
TEST_CASE("TestSocket", "[TestSocket1]") {

  Socket socket(std::make_shared<SocketContext>(std::make_shared<minifi::Configure>()), "localhost", 8183);
  REQUIRE(-1 == socket.initialize());
  REQUIRE("localhost" == socket.getHostname());
  socket.closeStream();

}

TEST_CASE("TestSocketWriteTest1", "[TestSocket2]") {

  Socket socket(std::make_shared<SocketContext>(std::make_shared<minifi::Configure>()), "localhost", 8183);
  REQUIRE(-1 == socket.initialize());

  socket.writeData(0, 0);

  std::vector<uint8_t> buffer;
  buffer.push_back('a');

  REQUIRE(-1 == socket.writeData(buffer, 1));

  socket.closeStream();

}

TEST_CASE("TestSocketWriteTest2", "[TestSocket3]") {

  std::vector<uint8_t> buffer;
  buffer.push_back('a');
  
  std::shared_ptr<SocketContext> socket_context = std::make_shared<SocketContext>(std::make_shared<minifi::Configure>());

  Socket server(socket_context, "localhost", 9183, 1);

  REQUIRE(-1 != server.initialize());

  Socket client(socket_context, "localhost", 9183);

  REQUIRE(-1 != client.initialize());

  REQUIRE(1 == client.writeData(buffer, 1));

  std::vector<uint8_t> readBuffer;
  readBuffer.resize(1);

  REQUIRE(1 == server.readData(readBuffer, 1));

  REQUIRE(readBuffer == buffer);

  server.closeStream();

  client.closeStream();

}

TEST_CASE("TestGetHostName", "[TestSocket4]") {

  REQUIRE(Socket::getMyHostName().length() > 0);

}

TEST_CASE("TestWriteEndian64", "[TestSocket4]") {

  std::vector<uint8_t> buffer;
  buffer.push_back('a');
  
  std::shared_ptr<SocketContext> socket_context = std::make_shared<SocketContext>(std::make_shared<minifi::Configure>());

  Socket server(socket_context, "localhost", 9183, 1);

  REQUIRE(-1 != server.initialize());

  Socket client(socket_context, "localhost", 9183);

  REQUIRE(-1 != client.initialize());

  uint64_t negative_one = -1;
  REQUIRE(8 == client.write(negative_one));

  uint64_t negative_two = 0;
  REQUIRE(8 == server.read(negative_two));

  REQUIRE(negative_two == negative_one);

  server.closeStream();

  client.closeStream();

}

TEST_CASE("TestWriteEndian32", "[TestSocket5]") {

  std::vector<uint8_t> buffer;
  buffer.push_back('a');

  std::shared_ptr<SocketContext> socket_context = std::make_shared<SocketContext>(std::make_shared<minifi::Configure>());
  
  Socket server(socket_context, "localhost", 9183, 1);

  REQUIRE(-1 != server.initialize());

  Socket client(socket_context, "localhost", 9183);

  REQUIRE(-1 != client.initialize());

  {
    uint32_t negative_one = -1;
    REQUIRE(4 == client.write(negative_one));

    uint32_t negative_two = 0;
    REQUIRE(4 == server.read(negative_two));

    REQUIRE(negative_two == negative_one);
  }

  {
    uint16_t negative_one = -1;
    REQUIRE(2 == client.write(negative_one));

    uint16_t negative_two = 0;
    REQUIRE(2 == server.read(negative_two));

    REQUIRE(negative_two == negative_one);
  }
  server.closeStream();

  client.closeStream();

}

TEST_CASE("TestSocketWriteTestAfterClose", "[TestSocket6]") {

  std::vector<uint8_t> buffer;
  buffer.push_back('a');

  std::shared_ptr<SocketContext> socket_context = std::make_shared<SocketContext>(std::make_shared<minifi::Configure>());
  
  Socket server(socket_context, "localhost", 9183, 1);

  REQUIRE(-1 != server.initialize());

  Socket client(socket_context, "localhost", 9183);

  REQUIRE(-1 != client.initialize());

  REQUIRE(1 == client.writeData(buffer, 1));

  std::vector<uint8_t> readBuffer;
  readBuffer.resize(1);

  REQUIRE(1 == server.readData(readBuffer, 1));

  REQUIRE(readBuffer == buffer);

  client.closeStream();

  REQUIRE(-1 == client.writeData(buffer, 1));

  server.closeStream();

}
