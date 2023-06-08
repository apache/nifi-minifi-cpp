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
#include <sys/stat.h>
#include <csignal>
#include <chrono>
#include <thread>
#include <filesystem>
#undef NDEBUG
#include <cassert>
#include <utility>
#include <memory>
#include <string>
#include "properties/Configure.h"
#include "io/tls/TLSSocket.h"
#include "SimpleSSLTestServer.h"

namespace minifi = org::apache::nifi::minifi;
class SimpleSSLTestServerTLSv1  : public SimpleSSLTestServer {
 public:
  SimpleSSLTestServerTLSv1(int port, const std::filesystem::path& key_dir)
      : SimpleSSLTestServer(TLS1_VERSION, port, key_dir) {
  }
};

class SimpleSSLTestServerTLSv1_1  : public SimpleSSLTestServer {
 public:
  SimpleSSLTestServerTLSv1_1(int port, const std::filesystem::path& key_dir)
      : SimpleSSLTestServer(TLS1_1_VERSION, port, key_dir) {
  }
};

class SimpleSSLTestServerTLSv1_2  : public SimpleSSLTestServer {
 public:
  SimpleSSLTestServerTLSv1_2(int port, const std::filesystem::path& key_dir)
      : SimpleSSLTestServer(TLS1_2_VERSION, port, key_dir) {
  }
};

class TLSClientSocketSupportedProtocolsTest {
 public:
  explicit TLSClientSocketSupportedProtocolsTest(std::filesystem::path key_dir)
      : key_dir_(std::move(key_dir)), configuration_(std::make_shared<minifi::Configure>()) {
  }

  void run() {
    configureSecurity();

    verifyTLSClientSocketExclusiveCompatibilityWithTLSv1_2();
  }


 protected:
  void configureSecurity() {
    host_ = minifi::io::Socket::getMyHostName();
    if (!key_dir_.empty()) {
      configuration_->set(minifi::Configure::nifi_remote_input_secure, "true");
      configuration_->set(minifi::Configure::nifi_security_client_certificate, (key_dir_ / "cn.crt.pem").string());
      configuration_->set(minifi::Configure::nifi_security_client_private_key, (key_dir_ / "cn.ckey.pem").string());
      configuration_->set(minifi::Configure::nifi_security_client_pass_phrase, (key_dir_ / "cn.pass").string());
      configuration_->set(minifi::Configure::nifi_security_client_ca_certificate, (key_dir_ / "nifi-cert.pem").string());
      configuration_->set(minifi::Configure::nifi_default_directory, key_dir_.string());
    }
  }

  void verifyTLSClientSocketExclusiveCompatibilityWithTLSv1_2() {
    verifyTLSProtocolCompatibility<SimpleSSLTestServerTLSv1>(false);
    verifyTLSProtocolCompatibility<SimpleSSLTestServerTLSv1_1>(false);
    verifyTLSProtocolCompatibility<SimpleSSLTestServerTLSv1_2>(true);
  }

  template <class TLSTestSever>
  void verifyTLSProtocolCompatibility(const bool should_be_compatible) {
    // bind to random port
    TLSTestSever server(0, key_dir_);
    server.waitForConnection();

    int port = server.getPort();

    const auto socket_context = std::make_shared<minifi::io::TLSContext>(configuration_);
    client_socket_ = std::make_unique<minifi::io::TLSSocket>(socket_context, host_, port, 0);
    const bool client_initialized_successfully = (client_socket_->initialize() == 0);
    assert(client_initialized_successfully == should_be_compatible);
    server.shutdownServer();
    assert(server.hadConnection() == should_be_compatible);
  }

  std::unique_ptr<minifi::io::TLSSocket> client_socket_;
  std::string host_;
  std::filesystem::path key_dir_;
  std::shared_ptr<minifi::Configure> configuration_;
};

static void sigpipe_handle(int) {
}

int main(int argc, char **argv) {
  std::string key_dir;
  if (argc > 1) {
    key_dir = argv[1];
  }
#ifndef WIN32
  signal(SIGPIPE, sigpipe_handle);
#endif

  TLSClientSocketSupportedProtocolsTest client_socket_supported_protocols_verifier(key_dir);

  client_socket_supported_protocols_verifier.run();

  return 0;
}
