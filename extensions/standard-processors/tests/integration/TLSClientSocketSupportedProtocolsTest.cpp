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
#include <signal.h>
#include <sys/stat.h>
#include <chrono>
#include <thread>
#undef NDEBUG
#include <cassert>
#include <utility>
#include <memory>
#include <string>
#include "properties/Configure.h"
#include "io/tls/TLSSocket.h"

#ifdef WIN32
#pragma comment(lib, "Ws2_32.lib")
using SocketDescriptor = SOCKET;
#else
using SocketDescriptor = int;
static constexpr SocketDescriptor INVALID_SOCKET = -1;
#endif /* WIN32 */


class SimpleSSLTestServer  {
 public:
  SimpleSSLTestServer(const SSL_METHOD* method, const std::string& port, const std::string& path)
      : port_(port), had_connection_(false) {
    ctx_ = SSL_CTX_new(method);
    configureContext(path);
    socket_descriptor_ = createSocket(std::stoi(port_));
  }

  ~SimpleSSLTestServer() {
      SSL_shutdown(ssl_);
      SSL_free(ssl_);
      SSL_CTX_free(ctx_);
  }

  void waitForConnection() {
    server_read_thread_ = std::thread([this]() -> void {
        SocketDescriptor client = accept(socket_descriptor_, nullptr, nullptr);
        if (client != INVALID_SOCKET) {
            ssl_ = SSL_new(ctx_);
            SSL_set_fd(ssl_, client);
            had_connection_ = (SSL_accept(ssl_) == 1);
        }
    });
  }

  void shutdownServer() {
#ifdef WIN32
    shutdown(socket_descriptor_, SD_BOTH);
    closesocket(socket_descriptor_);
#else
    shutdown(socket_descriptor_, SHUT_RDWR);
    close(socket_descriptor_);
#endif
    server_read_thread_.join();
  }

  bool hadConnection() const {
    return had_connection_;
  }

 private:
  SSL_CTX *ctx_ = nullptr;
  SSL* ssl_ = nullptr;
  std::string port_;
  SocketDescriptor socket_descriptor_;
  bool had_connection_;
  std::thread server_read_thread_;

  void configureContext(const std::string& path) {
    SSL_CTX_set_ecdh_auto(ctx_, 1);
    /* Set the key and cert */
    assert(SSL_CTX_use_certificate_file(ctx_, (path + "cn.crt.pem").c_str(), SSL_FILETYPE_PEM) == 1);
    assert(SSL_CTX_use_PrivateKey_file(ctx_, (path + "cn.ckey.pem").c_str(), SSL_FILETYPE_PEM) == 1);
  }

  static SocketDescriptor createSocket(int port) {
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    SocketDescriptor socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    assert(socket_descriptor >= 0);
    assert(bind(socket_descriptor, (struct sockaddr*)&addr, sizeof(addr)) >= 0);
    assert(listen(socket_descriptor, 1) >= 0);

    return socket_descriptor;
  }
};

class SimpleSSLTestServerTLSv1  : public SimpleSSLTestServer {
 public:
  SimpleSSLTestServerTLSv1(const std::string& port, const std::string& path)
      : SimpleSSLTestServer(TLSv1_server_method(), port, path) {
  }
};

class SimpleSSLTestServerTLSv1_1  : public SimpleSSLTestServer {
 public:
  SimpleSSLTestServerTLSv1_1(const std::string& port, const std::string& path)
      : SimpleSSLTestServer(TLSv1_1_server_method(), port, path) {
  }
};

class SimpleSSLTestServerTLSv1_2  : public SimpleSSLTestServer {
 public:
  SimpleSSLTestServerTLSv1_2(const std::string& port, const std::string& path)
      : SimpleSSLTestServer(TLSv1_2_server_method(), port, path) {
  }
};

class TLSClientSocketSupportedProtocolsTest {
 public:
  explicit TLSClientSocketSupportedProtocolsTest(const std::string& key_dir)
      : key_dir_(key_dir), configuration_(std::make_shared<minifi::Configure>()) {
  }

  void run() {
    configureSecurity();

    verifyTLSClientSocketExclusiveCompatibilityWithTLSv1_2();
  }


 protected:
  void configureSecurity() {
    host_ = org::apache::nifi::minifi::io::Socket::getMyHostName();
    port_ = "38777";
    if (!key_dir_.empty()) {
      configuration_->set(minifi::Configure::nifi_remote_input_secure, "true");
      configuration_->set(minifi::Configure::nifi_security_client_certificate, key_dir_ + "cn.crt.pem");
      configuration_->set(minifi::Configure::nifi_security_client_private_key, key_dir_ + "cn.ckey.pem");
      configuration_->set(minifi::Configure::nifi_security_client_pass_phrase, key_dir_ + "cn.pass");
      configuration_->set(minifi::Configure::nifi_security_client_ca_certificate, key_dir_ + "nifi-cert.pem");
      configuration_->set(minifi::Configure::nifi_default_directory, key_dir_);
    }
  }

  void verifyTLSClientSocketExclusiveCompatibilityWithTLSv1_2() {
    verifyTLSProtocolCompatibility<SimpleSSLTestServerTLSv1>(false);
    verifyTLSProtocolCompatibility<SimpleSSLTestServerTLSv1_1>(false);
    verifyTLSProtocolCompatibility<SimpleSSLTestServerTLSv1_2>(true);
  }

  template <class TLSTestSever>
  void verifyTLSProtocolCompatibility(const bool should_be_compatible) {
    TLSTestSever server(port_, key_dir_);
    server.waitForConnection();

    const auto socket_context = std::make_shared<org::apache::nifi::minifi::io::TLSContext>(configuration_);
    client_socket_ = std::make_unique<org::apache::nifi::minifi::io::TLSSocket>(socket_context, host_, std::stoi(port_), 0);
    const bool client_initialized_successfully = (client_socket_->initialize() == 0);
    assert(client_initialized_successfully == should_be_compatible);
    server.shutdownServer();
    assert(server.hadConnection() == should_be_compatible);
  }

 protected:
    std::unique_ptr<org::apache::nifi::minifi::io::TLSSocket> client_socket_;
    std::string host_;
    std::string port_;
    std::string key_dir_;
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
