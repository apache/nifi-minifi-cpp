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
#ifndef LIBMINIFI_INCLUDE_IO_TLS_TLSSOCKET_H_
#define LIBMINIFI_INCLUDE_IO_TLS_TLSSOCKET_H_

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "controllers/SSLContextService.h"
#include "core/expect.h"
#include "io/ClientSocket.h"
#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

#define TLS_GOOD 0
#define TLS_ERROR_CONTEXT 1
#define TLS_ERROR_PEM_MISSING 2
#define TLS_ERROR_CERT_MISSING 3
#define TLS_ERROR_KEY_ERROR 4
#define TLS_ERROR_CERT_ERROR 5

class OpenSSLInitializer {
 public:
  static OpenSSLInitializer *getInstance() {
    static OpenSSLInitializer openssl_initializer;
    return &openssl_initializer;
  }

  OpenSSLInitializer() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
  }
};

class TLSContext : public SocketContext {
 public:
  TLSContext(const std::shared_ptr<Configure> &configure, std::shared_ptr<minifi::controllers::SSLContextService> ssl_service = nullptr); // NOLINT

  virtual ~TLSContext() = default;

  SSL_CTX *getContext() {
    return ctx.get();
  }

  int16_t getError() {
    return error_value;
  }

  int16_t initialize(bool server_method = false);

 private:
  static void deleteContext(SSL_CTX* ptr) { SSL_CTX_free(ptr); }

  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<Configure> configure_;
  std::shared_ptr<minifi::controllers::SSLContextService> ssl_service_;
  std::unique_ptr<SSL_CTX, decltype(&deleteContext)> ctx;

  int16_t error_value;
};

class TLSSocket : public Socket {
 public:
  /**
   * Constructor that accepts host name, port and listeners. With this
   * contructor we will be creating a server socket
   * @param context the TLSContext
   * @param hostname our host name
   * @param port connecting port
   * @param listeners number of listeners in the queue
   */
  explicit TLSSocket(const std::shared_ptr<TLSContext> &context, const std::string &hostname, uint16_t port, uint16_t listeners);

  /**
   * Constructor that creates a client socket.
   * @param context the TLSContext
   * @param hostname hostname we are connecting to.
   * @param port port we are connecting to.
   */
  explicit TLSSocket(const std::shared_ptr<TLSContext> &context, const std::string &hostname, uint16_t port);

  /**
   * Move constructor.
   */
  TLSSocket(TLSSocket &&);

  TLSSocket& operator=(TLSSocket&&);

  ~TLSSocket() override;

  /**
   * Initializes the socket
   * @return result of the creation operation.
   */
  int initialize() override {
    return initialize(true);
  }

  int16_t initialize(bool blocking);

  /**
   * Attempt to select the socket file descriptor
   * @param msec timeout interval to wait
   * @returns file descriptor
   */
  int16_t select_descriptor(uint16_t msec) override;

  using Socket::read;
  using Socket::write;

  size_t read(uint8_t *buf, size_t buflen, bool retrieve_all_bytes) override;

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  size_t read(uint8_t *buf, size_t buflen) override;

  /**
   * Write value to the stream using uint8_t ptr
   * @param buf incoming buffer
   * @param buflen buffer to write
   *
   */
  int write(const uint8_t *value, int size) override;

  void close() override;

 protected:
  int writeData(const uint8_t *value, unsigned int size, int fd);

  SSL *get_ssl(int fd) {
    if (UNLIKELY(listeners_ > 0)) {
      std::lock_guard<std::mutex> lock(ssl_mutex_);
      return ssl_map_[fd];
    } else {
      return ssl_;
    }
  }

  void close_ssl(int fd);

  std::atomic<bool> connected_{ false };
  std::shared_ptr<TLSContext> context_;
  SSL* ssl_{ nullptr };
  std::mutex ssl_mutex_;
  std::map<int, SSL*> ssl_map_;
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_IO_TLS_TLSSOCKET_H_
