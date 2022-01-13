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

#undef LOAD_EXTENSIONS
#undef NDEBUG

#include <array>
#include <cassert>

#include "io/tls/TLSServerSocket.h"
#include "io/tls/TLSSocket.h"
#include "../../TestBase.h"
#include "../../Catch.h"
#include "../../SimpleSSLTestServer.h"
#include "../utils/IntegrationTestUtils.h"

using namespace std::literals::chrono_literals;

static std::shared_ptr<minifi::io::TLSContext> createContext(const std::filesystem::path& key_dir) {
  auto configuration = std::make_shared<minifi::Configure>();
  configuration->set(minifi::Configure::nifi_remote_input_secure, "true");
  configuration->set(minifi::Configure::nifi_security_client_certificate, (key_dir / "cn.crt.pem").string());
  configuration->set(minifi::Configure::nifi_security_client_private_key, (key_dir / "cn.ckey.pem").string());
  configuration->set(minifi::Configure::nifi_security_client_pass_phrase, (key_dir / "cn.pass").string());
  configuration->set(minifi::Configure::nifi_security_client_ca_certificate, (key_dir / "nifi-cert.pem").string());
  configuration->set(minifi::Configure::nifi_default_directory, key_dir.string());

  return std::make_shared<minifi::io::TLSContext>(configuration);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    throw std::logic_error("Specify the key directory");
  }
  std::filesystem::path key_dir(argv[1]);

  LogTestController::getInstance().setTrace<minifi::io::Socket>();
  LogTestController::getInstance().setTrace<minifi::io::TLSSocket>();
  LogTestController::getInstance().setTrace<minifi::io::TLSServerSocket>();
  LogTestController::getInstance().setTrace<minifi::io::TLSContext>();

  auto server = std::make_unique<SimpleSSLTestServer>(SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_3, 0, key_dir);
  int port = server->getPort();
  server->waitForConnection();

  std::string host = minifi::io::Socket::getMyHostName();

  auto client_ctx = createContext(key_dir);
  assert(client_ctx->initialize(false) == 0);

  minifi::io::TLSSocket client_socket(client_ctx, host, port);
  assert(client_socket.initialize() == 0);

  std::atomic_bool read_complete{false};

  std::thread read_thread{[&] {
    std::array<std::byte, 10> buffer{};
    auto read_count = client_socket.read(buffer);
    assert(read_count == 0);
    read_complete = true;
  }};

  server->shutdownServer();
  server.reset();

  assert(utils::verifyEventHappenedInPollTime(1s, [&] {return read_complete.load();}));

  read_thread.join();
}
