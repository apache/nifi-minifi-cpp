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
#include "io/tls/TLSServerSocket.h"

class SimpleSSLTestServer  {
 public:
  SimpleSSLTestServer(const SSL_METHOD* method, std::string port, std::string path)
      : port_(port) {
    ctx_ = SSL_CTX_new(method);
    configure_context(path);
  }

  ~SimpleSSLTestServer() {
      SSL_shutdown(ssl_);
      SSL_free(ssl_);
      SSL_CTX_free(ctx_);
  }

  void waitForConnection() {
    isRunning_ = true;
    sock_ = create_socket(std::stoi(port_));
    server_read_thread_ = std::thread([this]() -> void {
      while (isRunning_) {
        struct sockaddr_in addr;
        uint len = sizeof(addr);

        int client = accept(sock_, (struct sockaddr*)&addr, &len);
        ssl_ = SSL_new(ctx_);
        SSL_set_fd(ssl_, client);
        successful = (SSL_accept(ssl_) == 1);
      }
    });
  }

  bool isRunning_;
  std::thread server_read_thread_;
  int sock_;
  bool successful;

 private:
  SSL_CTX *ctx_;
  SSL* ssl_;
  std::string port_;
  uint16_t listeners_;

  void configure_context(std::string path) {
      SSL_CTX_set_ecdh_auto(ctx_, 1);
      /* Set the key and cert */
      assert(SSL_CTX_use_certificate_file(ctx_, (path + "cn.crt.pem").c_str(), SSL_FILETYPE_PEM) > 0);
      assert(SSL_CTX_use_PrivateKey_file(ctx_, (path + "cn.ckey.pem").c_str(), SSL_FILETYPE_PEM) > 0);
  }

  int create_socket(int port) {
    int s;
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
      perror("Unable to create socket");
      exit(EXIT_FAILURE);
    }

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      perror("Unable to bind");
      exit(EXIT_FAILURE);
    }

    if (listen(s, 1) < 0) {
      perror("Unable to listen");
      exit(EXIT_FAILURE);
    }

    return s;
  }
};

class SimpleSSLTestServerTLSv1  : public SimpleSSLTestServer {
 public:
  SimpleSSLTestServerTLSv1(std::string port, std::string path) : SimpleSSLTestServer(TLSv1_server_method(), port, path) {
  }
};

class SimpleSSLTestServerTLSv1_1  : public SimpleSSLTestServer {
 public:
  SimpleSSLTestServerTLSv1_1(std::string port, std::string path) : SimpleSSLTestServer(TLSv1_1_server_method(), port, path) {
  }
};

class SimpleSSLTestServerTLSv1_2  : public SimpleSSLTestServer {
 public:
  SimpleSSLTestServerTLSv1_2(std::string port, std::string path) : SimpleSSLTestServer(TLSv1_2_server_method(), port, path) {
  }
};

class TLSClientSocketSupportedProtocolsTest {
 public:
  TLSClientSocketSupportedProtocolsTest()
      : configuration_(std::make_shared<minifi::Configure>()) {
  }

  void run() {
    configureSecurity();

    runAssertions();
  }

  void setKeyDir(const std::string key_dir) {
    this->key_dir = key_dir;
  }

 protected:
  void configureSecurity() {
    host_ = org::apache::nifi::minifi::io::Socket::getMyHostName();
    port_ = "3684";
    if (!key_dir.empty()) {
      configuration_->set(minifi::Configure::nifi_remote_input_secure, "true");
      configuration_->set(minifi::Configure::nifi_security_client_certificate, key_dir + "cn.crt.pem");
      configuration_->set(minifi::Configure::nifi_security_client_private_key, key_dir + "cn.ckey.pem");
      configuration_->set(minifi::Configure::nifi_security_client_pass_phrase, key_dir + "cn.pass");
      configuration_->set(minifi::Configure::nifi_security_client_ca_certificate, key_dir + "nifi-cert.pem");
      configuration_->set(minifi::Configure::nifi_default_directory, key_dir);
    }
  }

  void runAssertions() {
    {
      SimpleSSLTestServerTLSv1 server(port_, key_dir);
      server.waitForConnection();

      std::shared_ptr<org::apache::nifi::minifi::io::TLSContext> socket_context = std::make_shared<org::apache::nifi::minifi::io::TLSContext>(configuration_);
      client_socket_ = std::make_shared<org::apache::nifi::minifi::io::TLSSocket>(socket_context, host_, std::stoi(port_), 0);
      assert(client_socket_->initialize() != 0);
      shutdown(server.sock_, SHUT_RD);
      close(server.sock_);
      server.isRunning_ = false;
      server.server_read_thread_.join();
    }
    {
      SimpleSSLTestServerTLSv1_1 server(port_, key_dir);
      server.waitForConnection();

      std::shared_ptr<org::apache::nifi::minifi::io::TLSContext> socket_context = std::make_shared<org::apache::nifi::minifi::io::TLSContext>(configuration_);
      client_socket_ = std::make_shared<org::apache::nifi::minifi::io::TLSSocket>(socket_context, host_, std::stoi(port_), 0);
      assert(client_socket_->initialize() != 0);
      shutdown(server.sock_, SHUT_RD);
      close(server.sock_);
      server.isRunning_ = false;
      server.server_read_thread_.join();
    }
    {
      SimpleSSLTestServerTLSv1_2 server(port_, key_dir);
      server.waitForConnection();

      std::shared_ptr<org::apache::nifi::minifi::io::TLSContext> socket_context = std::make_shared<org::apache::nifi::minifi::io::TLSContext>(configuration_);
      client_socket_ = std::make_shared<org::apache::nifi::minifi::io::TLSSocket>(socket_context, host_, std::stoi(port_), 0);
      assert(client_socket_->initialize() == 0);
      shutdown(server.sock_, SHUT_RD);
      close(server.sock_);
      server.isRunning_ = false;
      server.server_read_thread_.join();
    }
  }

 protected:
    std::shared_ptr<org::apache::nifi::minifi::io::TLSSocket> client_socket_;
    std::string host_;
    std::string port_;
    std::string key_dir;
    std::shared_ptr<minifi::Configure> configuration_;
};

static void sigpipe_handle(int /*x*/) {
}

int main(int argc, char **argv) {
  std::string key_dir, test_file_location;
  if (argc > 1) {
    key_dir = argv[1];
  }

#ifndef WIN32
  signal(SIGPIPE, sigpipe_handle);
#endif
  TLSClientSocketSupportedProtocolsTest clientSocketSupportedProtocolsTest;

  clientSocketSupportedProtocolsTest.setKeyDir(key_dir);

  clientSocketSupportedProtocolsTest.run();

  return 0;
}
