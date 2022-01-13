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

#pragma once

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <filesystem>
#include <string>
#include "io/tls/TLSSocket.h"

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using SocketDescriptor = SOCKET;
#else
using SocketDescriptor = int;
static constexpr SocketDescriptor INVALID_SOCKET = -1;
#endif /* WIN32 */

namespace minifi = org::apache::nifi::minifi;

class SimpleSSLTestServer  {
  struct SocketInitializer {
    SocketInitializer() {
#ifdef WIN32
      static WSADATA s_wsaData;
      const int iWinSockInitResult = WSAStartup(MAKEWORD(2, 2), &s_wsaData);
      if (0 != iWinSockInitResult) {
        throw std::runtime_error("Cannot initialize socket");
      }
#endif
    }
  };

 public:
  SimpleSSLTestServer(uint64_t disable_version, int port, const std::filesystem::path& key_dir)
      : port_(port), had_connection_(false) {
    static SocketInitializer socket_initializer{};
    minifi::io::OpenSSLInitializer::getInstance();
    ctx_ = SSL_CTX_new(TLS_server_method());
    SSL_CTX_set_options(ctx_, disable_version);
    configureContext(key_dir);
    socket_descriptor_ = createSocket(port_);
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

  int getPort() const {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    assert(getsockname(socket_descriptor_, (struct sockaddr*)&addr, &addr_len) == 0);
    return ntohs(addr.sin_port);
  }

 private:
  SSL_CTX *ctx_ = nullptr;
  SSL* ssl_ = nullptr;
  int port_;
  SocketDescriptor socket_descriptor_;
  bool had_connection_;
  std::thread server_read_thread_;

  void configureContext(const std::filesystem::path& key_dir) {
    SSL_CTX_set_ecdh_auto(ctx_, 1);
    /* Set the key and cert */
    assert(SSL_CTX_use_certificate_file(ctx_, (key_dir / "cn.crt.pem").string().c_str(), SSL_FILETYPE_PEM) == 1);
    assert(SSL_CTX_use_PrivateKey_file(ctx_, (key_dir / "cn.ckey.pem").string().c_str(), SSL_FILETYPE_PEM) == 1);
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
