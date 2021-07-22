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
#ifndef WIN32
#include <arpa/inet.h>
#endif
#include <signal.h>
#include <sys/stat.h>
#include <chrono>
#include <thread>
#include <cerrno>
#include <cinttypes>
#undef NDEBUG
#include <cassert>
#include <utility>
#include <memory>
#include <string>
#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
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

namespace {
const char* str_family(int family) {
  switch (family) {
    case AF_INET: return "AF_INET";
    case AF_INET6: return "AF_INET6";
    default: return "(n/a)";
  }
}

const char* str_socktype(int socktype) {
  switch (socktype) {
    case SOCK_STREAM: return "SOCK_STREAM";
    case SOCK_DGRAM: return "SOCK_DGRAM";
    default: return "(n/a)";
  }
}

const char* str_proto(int protocol) {
  switch (protocol) {
    case IPPROTO_TCP: return "IPPROTO_TCP";
    case IPPROTO_UDP: return "IPPROTO_UDP";
    case IPPROTO_IP: return "IPPROTO_IP";
    case IPPROTO_ICMP: return "IPPROTO_ICMP";
    default: return "(n/a)";
  }
}

std::string str_addr(const sockaddr* const sa) {
  char buf[128] = {0};
  switch (sa->sa_family) {
    case AF_INET: {
      sockaddr_in sin{};
      memcpy(&sin, sa, sizeof(sockaddr_in));
#ifdef WIN32
      const auto addr_str = InetNtop(AF_INET, &sin.sin_addr, buf, sizeof(buf));
#else
      const auto addr_str = inet_ntop(AF_INET, &sin.sin_addr, buf, sizeof(buf));
#endif
      if (!addr_str) {
        throw std::runtime_error{minifi::io::get_last_socket_error_message()};
      }
      return std::string{addr_str};
    }
    case AF_INET6: {
      sockaddr_in6 sin6{};
      memcpy(&sin6, sa, sizeof(sockaddr_in6));
#ifdef WIN32
      const auto addr_str = InetNtop(AF_INET, &sin6.sin6_addr, buf, sizeof(buf));
#else
      const auto addr_str = inet_ntop(AF_INET, &sin6.sin6_addr, buf, sizeof(buf));
#endif
      if (!addr_str) {
        throw std::runtime_error{minifi::io::get_last_socket_error_message()};
      }
      return std::string{addr_str};
    }
    default: return "(n/a)";
  }
}

void log_addrinfo(addrinfo* const ai, logging::Logger& logger) {
  logger.log_debug(".ai_family: %d %s\n", ai->ai_family, str_family(ai->ai_family));
  logger.log_debug(".ai_socktype: %d %s\n", ai->ai_socktype, str_socktype(ai->ai_socktype));
  logger.log_debug(".ai_protocol: %d %s\n", ai->ai_protocol, str_proto(ai->ai_protocol));
  logger.log_debug(".ai_addr: %s\n", str_addr(ai->ai_addr).c_str());
  logger.log_debug(".ai_addrlen: %" PRIu32 "\n", ai->ai_addrlen);
}
}  // namespace

class SimpleSSLTestClient  {
 public:
  SimpleSSLTestClient(const SSL_METHOD* method, const std::string& host, const std::string& port) :
    host_(host),
    port_(port) {
      ctx_ = SSL_CTX_new(method);
      sfd_ = openConnection(host_.c_str(), port_.c_str(), *logger_);
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
    const bool successful_connection = (status == 1);
    return successful_connection;
  }

 private:
  SSL_CTX *ctx_ = nullptr;
  SSL* ssl_ = nullptr;
  SocketDescriptor sfd_;
  std::string host_;
  std::string port_;
  gsl::not_null<std::shared_ptr<logging::Logger>> logger_{gsl::make_not_null(logging::LoggerFactory<SimpleSSLTestClient>::getLogger())};

  static SocketDescriptor openConnection(const char *host_name, const char *port, logging::Logger& logger) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo *addrs;
    const int status = getaddrinfo(host_name, port, &hints, &addrs);
    assert(status == 0);
    SocketDescriptor sfd = INVALID_SOCKET;
    for (struct addrinfo *addr = addrs; addr != nullptr; addr = addr->ai_next) {
      log_addrinfo(addr, logger);
      sfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
      if (sfd == INVALID_SOCKET) {
        logger.log_error("socket: %s\n", minifi::io::get_last_socket_error_message());
        continue;
      }
      const auto connect_result = connect(sfd, addr->ai_addr, addr->ai_addrlen);
      if (connect_result == 0) {
        break;
      } else {
        logger.log_error("connect to %s: %s\n", str_addr(addr->ai_addr), minifi::io::get_last_socket_error_message());
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
  SimpleSSLTestClientTLSv1(const std::string& host, const std::string& port)
      : SimpleSSLTestClient(TLSv1_client_method(), host, port) {
  }
};

class SimpleSSLTestClientTLSv1_1  : public SimpleSSLTestClient {
 public:
  SimpleSSLTestClientTLSv1_1(const std::string& host, const std::string& port)
      : SimpleSSLTestClient(TLSv1_1_client_method(), host, port) {
  }
};

class SimpleSSLTestClientTLSv1_2  : public SimpleSSLTestClient {
 public:
  SimpleSSLTestClientTLSv1_2(const std::string& host, const std::string& port)
      : SimpleSSLTestClient(TLSv1_2_client_method(), host, port) {
  }
};

class TLSServerSocketSupportedProtocolsTest {
 public:
    explicit TLSServerSocketSupportedProtocolsTest(const std::string& key_dir)
        : is_running_(false), key_dir_(key_dir), configuration_(std::make_shared<minifi::Configure>()) {
    }

    ~TLSServerSocketSupportedProtocolsTest() {
      shutdownServerSocket();
      server_socket_.reset();
    }

    void run() {
      configureSecurity();

      createServerSocket();

      verifyTLSServerSocketExclusiveCompatibilityWithTLSv1_2();

      shutdownServerSocket();
    }

 protected:
    void configureSecurity() {
      host_ = org::apache::nifi::minifi::io::Socket::getMyHostName();
      port_ = "38778";
      if (!key_dir_.empty()) {
        configuration_->set(minifi::Configure::nifi_remote_input_secure, "true");
        configuration_->set(minifi::Configure::nifi_security_client_certificate, key_dir_ + "cn.crt.pem");
        configuration_->set(minifi::Configure::nifi_security_client_private_key, key_dir_ + "cn.ckey.pem");
        configuration_->set(minifi::Configure::nifi_security_client_pass_phrase, key_dir_ + "cn.pass");
        configuration_->set(minifi::Configure::nifi_security_client_ca_certificate, key_dir_ + "nifi-cert.pem");
        configuration_->set(minifi::Configure::nifi_default_directory, key_dir_);
      }
    }

    void createServerSocket() {
      const auto socket_context = std::make_shared<org::apache::nifi::minifi::io::TLSContext>(configuration_);
      server_socket_ = std::make_unique<org::apache::nifi::minifi::io::TLSServerSocket>(socket_context, host_, std::stoi(port_), 3);
      assert(0 == server_socket_->initialize());

      is_running_ = true;
      auto handler = [](std::vector<uint8_t> *bytes_written) {
        const char contents[] = "hello world";
        *bytes_written = {std::begin(contents), std::end(contents)};
        assert(12 == bytes_written->size());
        return bytes_written->size();
      };
      server_socket_->registerCallback([this]{ return is_running_.load(); }, std::move(handler), std::chrono::milliseconds(50));
    }

    void verifyTLSServerSocketExclusiveCompatibilityWithTLSv1_2() {
      verifyTLSProtocolCompatibility<SimpleSSLTestClientTLSv1>(false);
      verifyTLSProtocolCompatibility<SimpleSSLTestClientTLSv1_1>(false);
      verifyTLSProtocolCompatibility<SimpleSSLTestClientTLSv1_2>(true);
    }

    template <class TLSTestClient>
    void verifyTLSProtocolCompatibility(bool should_be_compatible) {
      TLSTestClient client(host_, port_);
      assert(client.canConnect() == should_be_compatible);
    }

    void shutdownServerSocket() {
      is_running_ = false;
    }

    std::atomic<bool> is_running_;
    std::unique_ptr<org::apache::nifi::minifi::io::TLSServerSocket> server_socket_;
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

  TLSServerSocketSupportedProtocolsTest server_socket_supported_protocols_verifier(key_dir);

  server_socket_supported_protocols_verifier.run();

  return 0;
}
