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
#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock2.h>
#endif
#include "properties/Configure.h"
#include "io/tls/TLSSocket.h"
#include "io/tls/TLSServerSocket.h"

#ifdef WIN32
using SocketDescriptor = SOCKET;
#else
using SocketDescriptor = int;
static constexpr SocketDescriptor INVALID_SOCKET = -1;
#endif /* WIN32 */

class SimpleSSLTestClient  {
 public:
  SimpleSSLTestClient(const SSL_METHOD* method, std::string host, std::string port) :
    host_(host),
    port_(port) {
      ctx_ = SSL_CTX_new(method);
      sfd_ = openConnection(host_.c_str(), port_.c_str());
      if (ctx_ != nullptr)
        ssl_ = SSL_new(ctx_);
      if (ssl_ != nullptr)
        SSL_set_fd(ssl_, sfd_);
  }

  ~SimpleSSLTestClient() {
    SSL_free(ssl_);
#ifdef WIN32
    closesocket(sfd_);
#else
    close(sfd_);
#endif
    SSL_CTX_free(ctx_);
  }

  bool canConnect() {
    const int status = SSL_connect(ssl_);
    const bool successfulConnection = (status == 1);
    return successfulConnection;
  }

 private:
  SSL_CTX *ctx_;
  SSL* ssl_;
  SocketDescriptor sfd_;
  std::string host_;
  std::string port_;

  SocketDescriptor openConnection(const char *hostname, const char *port) {
    struct hostent *host;
    if ((host = gethostbyname(hostname)) == nullptr) {
        perror(hostname);
        exit(EXIT_FAILURE);
    }
    struct addrinfo hints = {0}, *addrs;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    const int status = getaddrinfo(hostname, port, &hints, &addrs);
    assert(status == 0);
    SocketDescriptor sfd = INVALID_SOCKET;
    for (struct addrinfo *addr = addrs; addr != nullptr; addr = addr->ai_next) {
        sfd = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
        if (sfd == INVALID_SOCKET) {
            continue;
        }
        if (connect(sfd, addr->ai_addr, addr->ai_addrlen) == 0) {
            break;
        }
        sfd = INVALID_SOCKET;
#ifdef WIN32
        closesocket(sfd);
#else
        close(sfd);
#endif
    }
    freeaddrinfo(addrs);
    assert(sfd != INVALID_SOCKET);
    return sfd;
  }
};

class SimpleSSLTestClientTLSv1  : public SimpleSSLTestClient {
 public:
  SimpleSSLTestClientTLSv1(std::string host, std::string port) : SimpleSSLTestClient(TLSv1_client_method(), host, port) {
  }
};

class SimpleSSLTestClientTLSv1_1  : public SimpleSSLTestClient {
 public:
  SimpleSSLTestClientTLSv1_1(std::string host, std::string port) : SimpleSSLTestClient(TLSv1_1_client_method(), host, port) {
  }
};

class SimpleSSLTestClientTLSv1_2  : public SimpleSSLTestClient {
 public:
  SimpleSSLTestClientTLSv1_2(std::string host, std::string port) : SimpleSSLTestClient(TLSv1_2_client_method(), host, port) {
  }
};

class TLSServerSocketSupportedProtocolsTest {
 public:
    TLSServerSocketSupportedProtocolsTest()
        : isRunning_{ false }, configuration_(std::make_shared<minifi::Configure>()) {
    }

    void run() {
      configureSecurity();

      createServerSocket();

      runAssertions();
    }

    void setKeyDir(const std::string key_dir) {
      this->key_dir = key_dir;
    }

 protected:
    void configureSecurity() {
      host_ = org::apache::nifi::minifi::io::Socket::getMyHostName();
      port_ = "38776";
      if (!key_dir.empty()) {
        configuration_->set(minifi::Configure::nifi_remote_input_secure, "true");
        configuration_->set(minifi::Configure::nifi_security_client_certificate, key_dir + "cn.crt.pem");
        configuration_->set(minifi::Configure::nifi_security_client_private_key, key_dir + "cn.ckey.pem");
        configuration_->set(minifi::Configure::nifi_security_client_pass_phrase, key_dir + "cn.pass");
        configuration_->set(minifi::Configure::nifi_security_client_ca_certificate, key_dir + "nifi-cert.pem");
        configuration_->set(minifi::Configure::nifi_default_directory, key_dir);
      }
    }

    void createServerSocket() {
      const auto socket_context = std::make_shared<org::apache::nifi::minifi::io::TLSContext>(configuration_);
      server_socket_ = std::make_shared<org::apache::nifi::minifi::io::TLSServerSocket>(socket_context, host_, std::stoi(port_), 3);
      assert(0 == server_socket_->initialize());

      isRunning_ = true;
      check = [this]() -> bool {
        return isRunning_;
      };
      handler = [this](std::vector<uint8_t> *b, int *size) {
        std::cout << "oh write!" << std::endl;
        b->reserve(20);
        memset(b->data(), 0x00, 20);
        memcpy(b->data(), "hello world", 11);
        *size = 20;
        return *size;
      };
      server_socket_->registerCallback(check, handler, std::chrono::milliseconds(50));
    }

    void runAssertions() {
      {
        SimpleSSLTestClientTLSv1 client(host_, port_);
        assert(!client.canConnect());
      }
      {
        SimpleSSLTestClientTLSv1_1 client(host_, port_);
        assert(!client.canConnect());
      }
      {
        SimpleSSLTestClientTLSv1_2 client(host_, port_);
        assert(client.canConnect());
      }
      isRunning_ = false;
    }

    std::function<bool()> check;
    std::function<int(std::vector<uint8_t>*b, int *size)> handler;
    std::atomic<bool> isRunning_;
    std::shared_ptr<org::apache::nifi::minifi::io::TLSServerSocket> server_socket_;
    std::string host_;
    std::string port_;
    std::string key_dir;
    std::shared_ptr<minifi::Configure> configuration_;
};

int main(int argc, char **argv) {
  std::string key_dir, test_file_location;
  if (argc > 1) {
    key_dir = argv[1];
  }

  TLSServerSocketSupportedProtocolsTest serverSocketSupportedProtocolsTest;

  serverSocketSupportedProtocolsTest.setKeyDir(key_dir);

  serverSocketSupportedProtocolsTest.run();

  return 0;
}
