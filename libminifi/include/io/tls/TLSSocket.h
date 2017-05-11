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
#ifndef LIBMINIFI_INCLUDE_IO_TLSSOCKET_H_
#define LIBMINIFI_INCLUDE_IO_TLSSOCKET_H_

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <atomic>
#include <cstdint>
#include "../ClientSocket.h"

#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

#define TLS_ERROR_CONTEXT 1
#define TLS_ERROR_PEM_MISSING 2
#define TLS_ERROR_CERT_MISSING 3
#define TLS_ERROR_KEY_ERROR 4
#define TLS_ERROR_CERT_ERROR 5

class OpenSSLInitializer
{
 public:
  static OpenSSLInitializer *getInstance() {
    OpenSSLInitializer* atomic_context = context_instance.load(
         std::memory_order_relaxed);
     std::atomic_thread_fence(std::memory_order_acquire);
     if (atomic_context == nullptr) {
       std::lock_guard<std::mutex> lock(context_mutex);
       atomic_context = context_instance.load(std::memory_order_relaxed);
       if (atomic_context == nullptr) {
         atomic_context = new OpenSSLInitializer();
         std::atomic_thread_fence(std::memory_order_release);
         context_instance.store(atomic_context, std::memory_order_relaxed);
       }
     }
     return atomic_context;
   }

  OpenSSLInitializer()
  {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
  }
 private:
  static std::atomic<OpenSSLInitializer*> context_instance;
  static std::mutex context_mutex;
};

class TLSContext: public SocketContext {

 public:
  TLSContext(const std::shared_ptr<Configure> &configure);
  
  virtual ~TLSContext() {
    if (0 != ctx)
      SSL_CTX_free(ctx);
  }

  SSL_CTX *getContext() {
    return ctx;
  }

  int16_t getError() {
    return error_value;
  }

  int16_t initialize();

 private:

  static int pemPassWordCb(char *buf, int size, int rwflag, void *configure) {
    std::string passphrase;

    if (static_cast<Configure*>(configure)->get(
        Configure::nifi_security_client_pass_phrase, passphrase)) {

      std::ifstream file(passphrase.c_str(), std::ifstream::in);
      if (!file.good()) {
        memset(buf, 0x00, size);
        return 0;
      }

      std::string password;
      password.assign((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
      file.close();
      memset(buf, 0x00, size);
      memcpy(buf, password.c_str(), password.length() - 1);

      return password.length() - 1;
    }
    return 0;
  }


  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<Configure> configure_;
  SSL_CTX *ctx;

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
  explicit TLSSocket(const std::shared_ptr<TLSContext> &context, const std::string &hostname, const uint16_t port,
                     const uint16_t listeners);

  /**
   * Constructor that creates a client socket.
   * @param context the TLSContext
   * @param hostname hostname we are connecting to.
   * @param port port we are connecting to.
   */
  explicit TLSSocket(const std::shared_ptr<TLSContext> &context, const std::string &hostname, const uint16_t port);

  /**
   * Move constructor.
   */
  explicit TLSSocket(const TLSSocket &&);

  virtual ~TLSSocket();

  /**
   * Initializes the socket
   * @return result of the creation operation.
   */
  int16_t initialize();

  /**
   * Attempt to select the socket file descriptor
   * @param msec timeout interval to wait
   * @returns file descriptor
   */
  virtual int16_t select_descriptor(const uint16_t msec);

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  virtual int readData(uint8_t *buf, int buflen);

  /**
   * Write value to the stream using std::vector
   * @param buf incoming buffer
   * @param buflen buffer to write
   *
   */
  int writeData(std::vector<uint8_t> &buf, int buflen);

  /**
   * Write value to the stream using uint8_t ptr
   * @param buf incoming buffer
   * @param buflen buffer to write
   *
   */
  int writeData(uint8_t *value, int size);

 protected:
  std::shared_ptr<TLSContext> context_;
  SSL* ssl;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_IO_TLSSOCKET_H_ */
