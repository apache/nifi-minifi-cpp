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
#include <openssl/ssl.h>
#include <openssl/err.h>
#ifdef WIN32
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#endif  // WIN32

#include <fstream>
#include <memory>
#include <utility>
#include <string>
#include <vector>

#include "io/tls/TLSSocket.h"
#include "properties/Configure.h"
#include "utils/StringUtils.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/gsl.h"
#include "utils/tls/TLSUtils.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::io {

TLSContext::TLSContext(const std::shared_ptr<Configure> &configure, std::shared_ptr<minifi::controllers::SSLContextService> ssl_service)
    : SocketContext(configure),
      configure_(configure),
      ssl_service_(std::move(ssl_service)),
      ctx(nullptr, deleteContext),
      error_value(TLS_GOOD) {
}

/**
 * The memory barrier is defined by the singleton
 */
int16_t TLSContext::initialize(bool server_method) {
  if (ctx) {
    return error_value;
  }

  logger_->log_debug("initializing %X", this);

  if (nullptr == OpenSSLInitializer::getInstance()) {
    return error_value;
  }

  std::string clientAuthStr;
  bool need_client_cert = (!configure_->get(Configure::nifi_security_need_ClientAuth, clientAuthStr) ||
       org::apache::nifi::minifi::utils::StringUtils::toBool(clientAuthStr).value_or(true));

  const SSL_METHOD *method;
  method = server_method ? TLS_server_method() : TLS_client_method();
  auto local_context = std::unique_ptr<SSL_CTX, decltype(&deleteContext)>(SSL_CTX_new(method), deleteContext);
  if (local_context == nullptr) {
    logger_->log_error("Could not create SSL context, error: %s.", std::strerror(errno));
    error_value = TLS_ERROR_CONTEXT;
    return error_value;
  }

  if (need_client_cert) {
    std::string certificate;
    std::string privatekey;
    std::string passphrase;
    std::string caCertificate;

    if (ssl_service_ != nullptr) {
      if (!ssl_service_->configure_ssl_context(local_context.get())) {
        error_value = TLS_ERROR_CERT_ERROR;
        return error_value;
      }
      ctx = std::move(local_context);
      error_value = TLS_GOOD;
      return 0;
    }

    if (!(configure_->get(Configure::nifi_security_client_certificate, certificate) && configure_->get(Configure::nifi_security_client_private_key, privatekey))) {
        logger_->log_error("Certificate and Private Key PEM file not configured, error: %s.", std::strerror(errno));
        error_value = TLS_ERROR_PEM_MISSING;
        return error_value;
    }
    // load certificates and private key in PEM format
    if (SSL_CTX_use_certificate_chain_file(local_context.get(), certificate.c_str()) <= 0) {
      logger_->log_error("Could not load certificate %s, for %X and %X error : %s", certificate, this, local_context.get(), std::strerror(errno));
      error_value = TLS_ERROR_CERT_MISSING;
      return error_value;
    }
    if (configure_->get(Configure::nifi_security_client_pass_phrase, passphrase)) {
      std::ifstream file(passphrase.c_str(), std::ifstream::in);
      if (file.good()) {
        // if we have been given a file copy that, otherwise treat the passphrase as a password
        std::string password;
        password.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        passphrase = password;
      }
      SSL_CTX_set_default_passwd_cb(local_context.get(), utils::tls::pemPassWordCb);
      SSL_CTX_set_default_passwd_cb_userdata(local_context.get(), &passphrase);
    }

    int retp = SSL_CTX_use_PrivateKey_file(local_context.get(), privatekey.c_str(), SSL_FILETYPE_PEM);
    if (retp != 1) {
      logger_->log_error("Could not create load private key,%i on %s error : %s", retp, privatekey, std::strerror(errno));
      error_value = TLS_ERROR_KEY_ERROR;
      return error_value;
    }
    // verify private key
    if (!SSL_CTX_check_private_key(local_context.get())) {
      logger_->log_error("Private key does not match the public certificate, error : %s", std::strerror(errno));
      error_value = TLS_ERROR_KEY_ERROR;
      return error_value;
    }
    // load CA certificates
    if (ssl_service_ != nullptr || configure_->get(Configure::nifi_security_client_ca_certificate, caCertificate)) {
      retp = SSL_CTX_load_verify_locations(local_context.get(), caCertificate.c_str(), nullptr);
      if (retp == 0) {
        logger_->log_error("Can not load CA certificate, Exiting, error : %s", std::strerror(errno));
        error_value = TLS_ERROR_CERT_ERROR;
        return error_value;
      }
    }

    logger_->log_debug("Load/Verify Client Certificate OK. for %X and %X", this, local_context.get());
  }
  ctx = std::move(local_context);
  error_value = TLS_GOOD;
  return 0;
}

TLSSocket::~TLSSocket() {
  TLSSocket::close();
}

void TLSSocket::close() {
  if (ssl_ != nullptr) {
    SSL_shutdown(ssl_);
    SSL_free(ssl_);
    ssl_ = nullptr;
  }
  Socket::close();
}

/**
 * Constructor that accepts host name, port and listeners. With this
 * contructor we will be creating a server socket
 * @param hostname our host name
 * @param port connecting port
 * @param listeners number of listeners in the queue
 */
TLSSocket::TLSSocket(const std::shared_ptr<TLSContext> &context, const std::string &hostname, const uint16_t port, const uint16_t listeners)
    : Socket(context, hostname, port, listeners) {
  logger_ = core::logging::LoggerFactory<TLSSocket>::getLogger();
  context_ = context;
}

TLSSocket::TLSSocket(const std::shared_ptr<TLSContext> &context, const std::string &hostname, const uint16_t port)
    : Socket(context, hostname, port, 0) {
  logger_ = core::logging::LoggerFactory<TLSSocket>::getLogger();
  context_ = context;
}

TLSSocket::TLSSocket(TLSSocket &&other) noexcept
    : Socket(std::move(other)),
      context_{ std::exchange(other.context_, nullptr) } {
  std::lock_guard<std::mutex> lg{ other.ssl_mutex_ };  // NOLINT(bugprone-use-after-move)

  connected_.exchange(other.connected_.load());  // NOLINT(bugprone-use-after-move)
  other.connected_.exchange(false);  // NOLINT(bugprone-use-after-move)
  ssl_ = std::exchange(other.ssl_, nullptr);  // NOLINT(bugprone-use-after-move)
  ssl_map_ = std::exchange(other.ssl_map_, {});  // NOLINT(bugprone-use-after-move)
}

TLSSocket& TLSSocket::operator=(TLSSocket&& other) noexcept {
  if (&other == this) return *this;
  this->Socket::operator=(static_cast<Socket&&>(other));
  std::lock_guard<std::mutex> lg{ other.ssl_mutex_ };
  connected_.exchange(other.connected_.load());
  other.connected_.exchange(false);
  context_ = std::exchange(other.context_, nullptr);
  ssl_ = std::exchange(other.ssl_, nullptr);
  ssl_map_ = std::exchange(other.ssl_map_, {});
  return *this;
}

int16_t TLSSocket::initialize(bool blocking) {
  const bool is_server = (listeners_ > 0);

  if (!blocking)
    setNonBlocking();
  logger_->log_trace("Initializing TLSSocket in %s mode", (is_server ? "server" : "client"));
  int16_t ret = context_->initialize(is_server);

  if (ret != 0) {
    logger_->log_warn("Failed to initialize SSL context!");
    return -1;
  }

  ret = Socket::initialize();
  if (ret != 0) {
    logger_->log_warn("Failed to initialise basic socket for TLS socket");
    return -1;
  }

  if (!is_server) {
    ssl_ = SSL_new(context_->getContext());
    SSL_set_fd(ssl_, socket_file_descriptor_);
    SSL_set_tlsext_host_name(ssl_, requested_hostname_.c_str());  // SNI extension
    connected_ = false;
    int rez = SSL_connect(ssl_);
    if (rez < 0) {
      ERR_print_errors_fp(stderr);
      int ssl_error = SSL_get_error(ssl_, rez);
      if (ssl_error == SSL_ERROR_WANT_WRITE) {
        logger_->log_trace("want write");
        return 0;
      } else if (ssl_error == SSL_ERROR_WANT_READ) {
        logger_->log_trace("want read");
        return 0;
      } else {
        logger_->log_error("SSL socket connect failed to %s %d", requested_hostname_, port_);
        close();
        return -1;
      }
    } else {
      connected_ = true;
      logger_->log_debug("SSL socket connect success to %s %d, on fd %d", requested_hostname_, port_, socket_file_descriptor_);
      return 0;
    }
  }

  return ret;
}

void TLSSocket::close_ssl(int fd) {
  FD_CLR(fd, &total_list_);  // clear from master set
  if (UNLIKELY(listeners_ > 0)) {
    std::lock_guard<std::mutex> lock(ssl_mutex_);
    auto fd_ssl = ssl_map_[fd];
    if (nullptr != fd_ssl) {
      SSL_shutdown(fd_ssl);
      SSL_free(fd_ssl);
      ssl_map_[fd] = nullptr;
    }
    utils::net::close_socket(fd);
  }
}

int16_t TLSSocket::select_descriptor(const uint16_t msec) {
  if (listeners_ == 0 && connected_) {
    return socket_file_descriptor_;
  }

  struct timeval tv;

  read_fds_ = total_list_;

  tv.tv_sec = msec / 1000;
  tv.tv_usec = (msec % 1000) * 1000;

  std::lock_guard<std::recursive_mutex> guard(selection_mutex_);

  if (msec > 0)
    select(socket_max_ + 1, &read_fds_, nullptr, nullptr, &tv);
  else
    select(socket_max_ + 1, &read_fds_, nullptr, nullptr, nullptr);

  for (int i = 0; i <= socket_max_; i++) {
    if (!FD_ISSET(i, &read_fds_)) continue;

    if (i != socket_file_descriptor_) {
      // data to be received on i
      return i;
    }

    // listener can accept a new connection
    if (listeners_ > 0) {
      const auto newfd = accept(socket_file_descriptor_, nullptr, nullptr);
      if (!valid_socket(newfd)) {
        logger_->log_error("accept: %s", utils::net::get_last_socket_error().message());
        return -1;
      }
      FD_SET(newfd, &total_list_);  // add to master set
      if (newfd > socket_max_) {    // keep track of the max
        socket_max_ = newfd;
      }
      auto ssl = SSL_new(context_->getContext());
      SSL_set_fd(ssl, newfd);
      ssl_map_[newfd] = ssl;
      auto accept_value = SSL_accept(ssl);
      if (accept_value > 0) {
        logger_->log_trace("Accepted on %d", newfd);
        return newfd;
      }
      int ssl_err = SSL_get_error(ssl, accept_value);
      logger_->log_error("Could not accept %d, error code %d", newfd, ssl_err);
      close_ssl(newfd);
      return -1;
    }
    if (!connected_) {
      int rez = SSL_connect(ssl_);
      if (rez < 0) {
        ERR_print_errors_fp(stderr);
        int ssl_error = SSL_get_error(ssl_, rez);
        if (ssl_error == SSL_ERROR_WANT_WRITE) {
          logger_->log_trace("want write");
          return socket_file_descriptor_;
        } else if (ssl_error == SSL_ERROR_WANT_READ) {
          logger_->log_trace("want read");
          return socket_file_descriptor_;
        } else {
          logger_->log_error("SSL socket connect failed (%d) to %s %d", ssl_error, requested_hostname_, port_);
          close();
          return -1;
        }
      }
      connected_ = true;
      logger_->log_debug("SSL socket connect success to %s %d, on fd %d", requested_hostname_, port_, socket_file_descriptor_);
      return socket_file_descriptor_;
    }
    return socket_file_descriptor_;
    // we have a new connection
  }

  bool is_server = listeners_ > 0;

  if (listeners_ == 0) {
    return socket_file_descriptor_;
  }

  logger_->log_trace("%s Could not find a suitable file descriptor or select timed out", is_server ? "Server:" : "Client:");

  return -1;
}

size_t TLSSocket::read(gsl::span<std::byte> buffer, bool) {
  size_t total_read = 0;
  int status = 0;
  int loc = 0;
  auto* buf = buffer.data();
  auto buflen = buffer.size();
  int16_t fd = select_descriptor(1000);
  if (fd < 0) {
    close();
    return STREAM_ERROR;
  }
  auto fd_ssl = get_ssl(fd);
  if (IsNullOrEmpty(fd_ssl)) {
    return STREAM_ERROR;
  }
  if (!SSL_pending(fd_ssl)) {
    return 0;
  }
  while (buflen) {
    if (fd <= 0) {
      return STREAM_ERROR;
    }
    int sslStatus;
    do {
      const auto ssl_read_size = gsl::narrow<int>(std::min(buflen, gsl::narrow<size_t>(std::numeric_limits<int>::max())));
      status = SSL_read(fd_ssl, buf + loc, ssl_read_size);
      sslStatus = SSL_get_error(fd_ssl, status);
    } while (status < 0 && sslStatus == SSL_ERROR_WANT_READ && SSL_pending(fd_ssl));

    if (status < 0) break;

    buflen -= gsl::narrow<size_t>(status);
    loc += status;
    total_read += gsl::narrow<size_t>(status);
  }

  return total_read;
}

size_t TLSSocket::writeData(const uint8_t *value, size_t size, int fd) {
  size_t bytes = 0;
  auto fd_ssl = get_ssl(fd);
  if (IsNullOrEmpty(fd_ssl)) {
    return STREAM_ERROR;
  }
  while (bytes < size) {
    const auto sent = SSL_write(fd_ssl, value + bytes, gsl::narrow<int>(size - bytes));
    // check for errors
    if (sent < 0) {
      int ret = 0;
      ret = SSL_get_error(fd_ssl, sent);
      logger_->log_trace("WriteData socket %d send failed %s %d", fd, strerror(errno), ret);
      return STREAM_ERROR;
    }
    logger_->log_trace("WriteData socket %d send succeed %d", fd, sent);
    bytes += gsl::narrow<size_t>(sent);
  }
  return size;
}

size_t TLSSocket::write(const uint8_t *value, size_t size) {
  const int fd = select_descriptor(1000);
  if (fd < 0) {
    close();
    return STREAM_ERROR;
  }
  return writeData(value, size, fd);
}

size_t TLSSocket::read(gsl::span<std::byte> buffer) {
  size_t total_read = 0;
  int status = 0;
  auto* buf = buffer.data();
  auto buflen = buffer.size();
  while (buflen) {
    const int16_t fd = select_descriptor(1000);
    if (fd < 0) {
      close();
      return STREAM_ERROR;
    }

    int sslStatus;
    do {
      const auto fd_ssl = get_ssl(fd);
      if (IsNullOrEmpty(fd_ssl)) {
        return STREAM_ERROR;
      }
      const auto ssl_read_size = gsl::narrow<int>(std::min(buflen, gsl::narrow<size_t>(std::numeric_limits<int>::max())));
      status = SSL_read(fd_ssl, buf, ssl_read_size);
      sslStatus = SSL_get_error(fd_ssl, status);
    } while (status <= 0 && sslStatus == SSL_ERROR_WANT_READ);

    if (status <= 0)
      break;

    buflen -= gsl::narrow<size_t>(status);
    buf += status;
    total_read += gsl::narrow<size_t>(status);
  }

  return total_read;
}

}  // namespace org::apache::nifi::minifi::io
