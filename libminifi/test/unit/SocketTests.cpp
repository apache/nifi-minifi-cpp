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

#include <thread>
#include <random>
#include <chrono>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include "../TestBase.h"
#include "io/StreamFactory.h"
#include "io/ClientSocket.h"
#include "io/ServerSocket.h"
#include "io/tls/TLSSocket.h"
#include "utils/ThreadPool.h"

TEST_CASE("TestSocket", "[TestSocket1]") {
  org::apache::nifi::minifi::io::Socket socket(std::make_shared<org::apache::nifi::minifi::io::SocketContext>(std::make_shared<minifi::Configure>()), "localhost", 8183);
  REQUIRE(-1 == socket.initialize());
  REQUIRE(socket.getHostname().rfind("localhost", 0) == 0);
  socket.closeStream();
}

TEST_CASE("TestSocketWriteTest1", "[TestSocket2]") {
  org::apache::nifi::minifi::io::Socket socket(std::make_shared<org::apache::nifi::minifi::io::SocketContext>(std::make_shared<minifi::Configure>()), "localhost", 8183);
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
  std::shared_ptr<org::apache::nifi::minifi::io::SocketContext> socket_context = std::make_shared<org::apache::nifi::minifi::io::SocketContext>(std::make_shared<minifi::Configure>());

  org::apache::nifi::minifi::io::ServerSocket server(socket_context, "localhost", 9183, 1);

  REQUIRE(-1 != server.initialize());

  org::apache::nifi::minifi::io::Socket client(socket_context, "localhost", 9183);

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
  REQUIRE(org::apache::nifi::minifi::io::Socket::getMyHostName().length() > 0);
}

TEST_CASE("TestWriteEndian64", "[TestSocket5]") {
  std::vector<uint8_t> buffer;
  buffer.push_back('a');
  std::shared_ptr<org::apache::nifi::minifi::io::SocketContext> socket_context = std::make_shared<org::apache::nifi::minifi::io::SocketContext>(std::make_shared<minifi::Configure>());

  org::apache::nifi::minifi::io::ServerSocket server(socket_context, "localhost", 9183, 1);

  REQUIRE(-1 != server.initialize());

  org::apache::nifi::minifi::io::Socket client(socket_context, "localhost", 9183);

  REQUIRE(-1 != client.initialize());

  uint64_t negative_one = -1;
  REQUIRE(8 == client.write(negative_one));

  uint64_t negative_two = 0;
  REQUIRE(8 == server.read(negative_two));

  REQUIRE(negative_two == negative_one);

  server.closeStream();

  client.closeStream();
}

TEST_CASE("TestWriteEndian32", "[TestSocket6]") {
  std::vector<uint8_t> buffer;
  buffer.push_back('a');

  std::shared_ptr<org::apache::nifi::minifi::io::SocketContext> socket_context = std::make_shared<org::apache::nifi::minifi::io::SocketContext>(std::make_shared<minifi::Configure>());

  org::apache::nifi::minifi::io::ServerSocket server(socket_context, "localhost", 9183, 1);
  REQUIRE(-1 != server.initialize());

  org::apache::nifi::minifi::io::Socket client(socket_context, "localhost", 9183);

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

TEST_CASE("TestSocketWriteTestAfterClose", "[TestSocket7]") {
  std::vector<uint8_t> buffer;
  buffer.push_back('a');

  std::shared_ptr<org::apache::nifi::minifi::io::SocketContext> socket_context = std::make_shared<org::apache::nifi::minifi::io::SocketContext>(std::make_shared<minifi::Configure>());

  org::apache::nifi::minifi::io::ServerSocket server(socket_context, "localhost", 9183, 1);

  REQUIRE(-1 != server.initialize());

  org::apache::nifi::minifi::io::Socket client(socket_context, "localhost", 9183);

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

std::atomic<uint8_t> counter;
std::mt19937_64 seed { std::random_device { }() };
bool createSocket() {
  int mine = counter++;
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();

  std::uniform_int_distribution<> distribution { 10, 100 };
  std::this_thread::sleep_for(std::chrono::milliseconds { distribution(seed) });

  for (int i = 0; i < 50; i++) {
    std::shared_ptr<org::apache::nifi::minifi::io::TLSContext> socketA = std::make_shared<org::apache::nifi::minifi::io::TLSContext>(configuration);
    socketA->initialize();
  }

  return true;
}
/**
 * MINIFI-320 was created to address reallocations within TLSContext
 * This test will create 20 threads that attempt to create contexts
 * to ensure we no longer see the segfaults.
 */
TEST_CASE("TestTLSContextCreation", "[TestSocket8]") {
  utils::ThreadPool<bool> pool(20, true);

  std::vector<std::future<bool>> futures;
  for (int i = 0; i < 20; i++) {
    std::function<bool()> f_ex = createSocket;
    utils::Worker<bool> functor(f_ex, "id");
    std::future<bool> fut;
    REQUIRE(true == pool.execute(std::move(functor), fut));
    futures.push_back(std::move(fut));
  }
  pool.start();
  for (auto &&future : futures) {
    future.wait();
  }

  REQUIRE(20 == counter.load());
}

/**
 * MINIFI-329 was created in regards to an option existing but not
 * being properly evaluated.
 */
TEST_CASE("TestTLSContextCreation2", "[TestSocket9]") {
  std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::Configure>();
  configure->set("nifi.remote.input.secure", "false");
  auto factory = minifi::io::StreamFactory::getInstance(configure);
  std::string host = "localhost";
  minifi::io::Socket *socket = factory->createSocket(host, 10001).release();
  minifi::io::TLSSocket *tls = dynamic_cast<minifi::io::TLSSocket*>(socket);
  REQUIRE(tls == nullptr);
}

/**
 * MINIFI-329 was created in regards to an option existing but not
 * being properly evaluated.
 */
TEST_CASE("TestTLSContextCreationNullptr", "[TestSocket10]") {
  std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::Configure>();
  configure->set("nifi.remote.input.secure", "false");
  auto factory = minifi::io::StreamFactory::getInstance(configure);
  std::string host = "localhost";
  minifi::io::Socket *socket = factory->createSecureSocket(host, 10001, nullptr).release();
  minifi::io::TLSSocket *tls = dynamic_cast<minifi::io::TLSSocket*>(socket);
  REQUIRE(tls == nullptr);
}

