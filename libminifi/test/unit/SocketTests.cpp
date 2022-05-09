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
#include "../Catch.h"
#include "io/StreamFactory.h"
#include "io/Sockets.h"
#include "utils/ThreadPool.h"
#include "properties/Configuration.h"

namespace minifi = org::apache::nifi::minifi;
namespace io = minifi::io;
using io::Socket;

TEST_CASE("TestSocket", "[TestSocket1]") {
  Socket socket(std::make_shared<io::SocketContext>(std::make_shared<minifi::Configure>()), Socket::getMyHostName(), 8183);
  REQUIRE(-1 == socket.initialize());
  REQUIRE(socket.getHostname().rfind(Socket::getMyHostName(), 0) == 0);
  socket.close();
}

TEST_CASE("TestSocketWriteTest1", "[TestSocket2]") {
  Socket socket(std::make_shared<io::SocketContext>(std::make_shared<minifi::Configure>()), Socket::getMyHostName(), 8183);
  REQUIRE(-1 == socket.initialize());

  socket.write((const uint8_t*)nullptr, 0);

  std::vector<uint8_t> buffer;
  buffer.push_back('a');

  REQUIRE(io::isError(socket.write(buffer, 1)));

  socket.close();
}

TEST_CASE("TestSocketWriteTest2", "[TestSocket3]") {
  std::vector buffer = { static_cast<std::byte>('a') };
  std::shared_ptr<io::SocketContext> socket_context = std::make_shared<io::SocketContext>(std::make_shared<minifi::Configure>());
  io::ServerSocket server(socket_context, Socket::getMyHostName(), 9183, 1);

  REQUIRE(-1 != server.initialize());

  Socket client(socket_context, Socket::getMyHostName(), 9183);
  REQUIRE(-1 != client.initialize());
  REQUIRE(1 == client.write(buffer));

  std::vector<std::byte> readBuffer;
  readBuffer.resize(1);
  REQUIRE(1 == server.read(readBuffer));
  REQUIRE(readBuffer == buffer);

  server.close();
  client.close();
}

TEST_CASE("TestGetHostName", "[TestSocket4]") {
  REQUIRE(Socket::getMyHostName().length() > 0);
}

TEST_CASE("TestWriteEndian64", "[TestSocket5]") {
  std::vector<uint8_t> buffer;
  buffer.push_back('a');
  std::shared_ptr<io::SocketContext> socket_context = std::make_shared<io::SocketContext>(std::make_shared<minifi::Configure>());

  io::ServerSocket server(socket_context, Socket::getMyHostName(), 9183, 1);

  REQUIRE(-1 != server.initialize());

  Socket client(socket_context, Socket::getMyHostName(), 9183);

  REQUIRE(-1 != client.initialize());

  uint64_t negative_one = -1;
  REQUIRE(8 == client.write(negative_one));

  uint64_t negative_two = 0;
  REQUIRE(8 == server.read(negative_two));

  REQUIRE(negative_two == negative_one);

  server.close();

  client.close();
}

TEST_CASE("TestWriteEndian32", "[TestSocket6]") {
  std::vector<uint8_t> buffer;
  buffer.push_back('a');

  std::shared_ptr<io::SocketContext> socket_context = std::make_shared<io::SocketContext>(std::make_shared<minifi::Configure>());

  io::ServerSocket server(socket_context, Socket::getMyHostName(), 9183, 1);
  REQUIRE(-1 != server.initialize());

  Socket client(socket_context, Socket::getMyHostName(), 9183);

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
  server.close();

  client.close();
}

TEST_CASE("TestSocketWriteTestAfterClose", "[TestSocket7]") {
  std::vector buffer = {static_cast<std::byte>('a')};
  std::shared_ptr<io::SocketContext> socket_context = std::make_shared<io::SocketContext>(std::make_shared<minifi::Configure>());
  io::ServerSocket server(socket_context, Socket::getMyHostName(), 9183, 1);
  REQUIRE(-1 != server.initialize());

  Socket client(socket_context, Socket::getMyHostName(), 9183);
  REQUIRE(-1 != client.initialize());
  REQUIRE(1 == client.write(buffer));

  std::vector<std::byte> readBuffer;
  readBuffer.resize(1);
  REQUIRE(1 == server.read(readBuffer));
  REQUIRE(readBuffer == buffer);

  client.close();
  REQUIRE(io::isError(client.write(buffer)));
  server.close();
}

#ifdef OPENSSL_SUPPORT
std::atomic<uint8_t> counter;
std::mt19937_64 seed { std::random_device { }() };
bool createSocket() {
  counter++;
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();

  std::uniform_int_distribution<> distribution { 10, 100 };
  std::this_thread::sleep_for(std::chrono::milliseconds { distribution(seed) });

  for (int i = 0; i < 50; i++) {
    std::shared_ptr<io::TLSContext> socketA = std::make_shared<io::TLSContext>(configuration);
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
    pool.execute(std::move(functor), fut);  // NOLINT(bugprone-use-after-move)
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
  configure->set(minifi::Configuration::nifi_remote_input_secure, "false");
  auto factory = io::StreamFactory::getInstance(configure);
  std::string host = Socket::getMyHostName();
  Socket *socket = factory->createSocket(host, 10001).release();
  io::TLSSocket *tls = dynamic_cast<io::TLSSocket*>(socket);
  REQUIRE(tls == nullptr);
}

/**
 * MINIFI-329 was created in regards to an option existing but not
 * being properly evaluated.
 */
TEST_CASE("TestTLSContextCreationNullptr", "[TestSocket10]") {
  std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::Configure>();
  configure->set(minifi::Configuration::nifi_remote_input_secure, "false");
  auto factory = io::StreamFactory::getInstance(configure);
  std::string host = Socket::getMyHostName();
  io::Socket *socket = factory->createSecureSocket(host, 10001, nullptr).release();
  io::TLSSocket *tls = dynamic_cast<minifi::io::TLSSocket*>(socket);
  REQUIRE(tls == nullptr);
}
#endif  // OPENSSL_SUPPORT
