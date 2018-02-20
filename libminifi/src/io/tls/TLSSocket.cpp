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
#include <memory>
#include <utility>
#include <string>
#include <vector>
#include "io/tls/TLSSocket.h"
#include "properties/Configure.h"
#include "utils/StringUtils.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

std::atomic<OpenSSLInitializer*> OpenSSLInitializer::context_instance;
std::mutex OpenSSLInitializer::context_mutex;

TLSContext::TLSContext(const std::shared_ptr<Configure> &configure, const std::shared_ptr<minifi::controllers::SSLContextService> &ssl_service)
    : SocketContext(configure),
      error_value(0),
      ctx(0),
      logger_(logging::LoggerFactory<TLSContext>::getLogger()),
      configure_(configure),
      ssl_service_(ssl_service) {
}

/**
 * The memory barrier is defined by the singleton
 */
int16_t TLSContext::initialize(bool server_method) {
  if (ctx != 0) {
    return error_value;
  }

  logger_->log_debug("initializing %X", this);

  if (nullptr == OpenSSLInitializer::getInstance()) {
    return error_value;
  }

  std::string clientAuthStr;
  bool needClientCert = true;
  if (!(configure_->get(Configure::nifi_security_need_ClientAuth, clientAuthStr) && org::apache::nifi::minifi::utils::StringUtils::StringToBool(clientAuthStr, needClientCert))) {
    needClientCert = true;
  }
  const SSL_METHOD *method;
  method = server_method ? TLSv1_2_server_method() : TLSv1_2_client_method();
  ctx = SSL_CTX_new(method);
  if (ctx == NULL) {
    logger_->log_error("Could not create SSL context, error: %s.", std::strerror(errno));
    error_value = TLS_ERROR_CONTEXT;
    return error_value;
  }
  if (needClientCert) {
    std::string certificate;
    std::string privatekey;
    std::string passphrase;
    std::string caCertificate;

    if (ssl_service_ != nullptr) {
      certificate = ssl_service_->getCertificateFile();
      privatekey = ssl_service_->getPrivateKeyFile();
      caCertificate = ssl_service_->getCACertificate();
      passphrase = ssl_service_->getPassphrase();
    } else {
      if (!(configure_->get(Configure::nifi_security_client_certificate, certificate) && configure_->get(Configure::nifi_security_client_private_key, privatekey))) {
        logger_->log_error("Certificate and Private Key PEM file not configured, error: %s.", std::strerror(errno));
        error_value = TLS_ERROR_PEM_MISSING;
        return error_value;
      }
    }
    // load certificates and private key in PEM format
    if (SSL_CTX_use_certificate_file(ctx, certificate.c_str(), SSL_FILETYPE_PEM) <= 0) {
      logger_->log_error("Could not load certificate %s, for %X and %X error : %s", certificate, this, ctx, std::strerror(errno));
      error_value = TLS_ERROR_CERT_MISSING;
      return error_value;
    }
    if (ssl_service_ != nullptr) {
      // if the private key has passphase
      SSL_CTX_set_default_passwd_cb(ctx, pemPassWordCb);
      SSL_CTX_set_default_passwd_cb_userdata(ctx, static_cast<void*>(const_cast<char*>(passphrase.c_str())));
    } else {
      if (configure_->get(Configure::nifi_security_client_pass_phrase, passphrase)) {
        std::ifstream file(passphrase.c_str(), std::ifstream::in);
        if (!file.good()) {
          return error_value;
        }

        std::string password;
        password.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        passphrase = password;
        SSL_CTX_set_default_passwd_cb(ctx, pemPassWordCb);
        SSL_CTX_set_default_passwd_cb_userdata(ctx, static_cast<void*>(const_cast<char*>(passphrase.c_str())));
      }
    }

    int retp = SSL_CTX_use_PrivateKey_file(ctx, privatekey.c_str(), SSL_FILETYPE_PEM);
    if (retp != 1) {
      logger_->log_error("Could not create load private key,%i on %s error : %s", retp, privatekey, std::strerror(errno));
      error_value = TLS_ERROR_KEY_ERROR;
      return error_value;
    }
    // verify private key
    if (!SSL_CTX_check_private_key(ctx)) {
      logger_->log_error("Private key does not match the public certificate, error : %s", std::strerror(errno));
      error_value = TLS_ERROR_KEY_ERROR;
      return error_value;
    }
    // load CA certificates
    if (ssl_service_ != nullptr || configure_->get(Configure::nifi_security_client_ca_certificate, caCertificate)) {
      retp = SSL_CTX_load_verify_locations(ctx, caCertificate.c_str(), 0);
      if (retp == 0) {
        logger_->log_error("Can not load CA certificate, Exiting, error : %s", std::strerror(errno));
        error_value = TLS_ERROR_CERT_ERROR;
        return error_value;
      }
    }

    logger_->log_debug("Load/Verify Client Certificate OK. for %X and %X", this, ctx);
  }
  return 0;
}

TLSSocket::~TLSSocket() {
  if (ssl_ != 0) {
    SSL_free(ssl_);
    ssl_ = nullptr;
  }
  closeStream();
}
/**
 * Constructor that accepts host name, port and listeners. With this
 * contructor we will be creating a server socket
 * @param hostname our host name
 * @param port connecting port
 * @param listeners number of listeners in the queue
 */
TLSSocket::TLSSocket(const std::shared_ptr<TLSContext> &context, const std::string &hostname, const uint16_t port, const uint16_t listeners)
    : Socket(context, hostname, port, listeners),
      ssl_(0),
      logger_(logging::LoggerFactory<TLSSocket>::getLogger()) {
  context_ = context;
}

TLSSocket::TLSSocket(const std::shared_ptr<TLSContext> &context, const std::string &hostname, const uint16_t port)
    : Socket(context, hostname, port, 0),
      ssl_(0),
      logger_(logging::LoggerFactory<TLSSocket>::getLogger()) {
  context_ = context;
}

TLSSocket::TLSSocket(const TLSSocket &&d)
    : Socket(std::move(d)),
      ssl_(0),
      logger_(std::move(d.logger_)) {
  context_ = d.context_;
}

int16_t TLSSocket::initialize(bool blocking) {
  bool is_server = false;
  if (listeners_ > 0)
    is_server = true;

  if (!blocking)
    setNonBlocking();
  logger_->log_trace("Initializing TLSSocket %d", is_server);
  int16_t ret = context_->initialize(is_server);
  Socket::initialize();

  if (!ret && listeners_ == 0) {
    // we have s2s secure config
    ssl_ = SSL_new(context_->getContext());
    SSL_set_fd(ssl_, socket_file_descriptor_);
    connected_ = false;
    int rez = SSL_connect(ssl_);
    if (rez < 0) {
      ERR_print_errors_fp(stderr);
      int ssl_error = SSL_get_error(ssl_, rez);
      if (ssl_error == SSL_ERROR_WANT_WRITE) {
        logger_->log_trace("want read");
        return 0;
      } else if (ssl_error == SSL_ERROR_WANT_READ) {
        logger_->log_trace("want read");
        return 0;
      } else {
        return -1;
      }
      logger_->log_error("SSL socket connect failed to %s %d", requested_hostname_, port_);
      SSL_free(ssl_);
      ssl_ = NULL;
      close(socket_file_descriptor_);
      return -1;
    } else {
      connected_ = true;
      logger_->log_debug("SSL socket connect success to %s %d, on fd %d", requested_hostname_, port_, socket_file_descriptor_);
      return 0;
    }
  }

  return ret;
}

void TLSSocket::close_ssl(int fd) {
  FD_CLR(fd, &total_list_);  // add to master set
  if (UNLIKELY(listeners_ > 0)) {
    std::lock_guard<std::mutex> lock(ssl_mutex_);
    auto fd_ssl = ssl_map_[fd];
    if (nullptr != fd_ssl) {
      SSL_free(fd_ssl);
      ssl_map_[fd] = nullptr;
      close(fd);
    }
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
    select(socket_max_ + 1, &read_fds_, NULL, NULL, &tv);
  else
    select(socket_max_ + 1, &read_fds_, NULL, NULL, NULL);

  for (int i = 0; i <= socket_max_; i++) {
    if (FD_ISSET(i, &read_fds_)) {
      if (i == socket_file_descriptor_) {
        if (listeners_ > 0) {
          struct sockaddr_in remoteaddr;  // client address
          socklen_t addrlen = sizeof remoteaddr;
          int newfd = accept(socket_file_descriptor_, (struct sockaddr *) &remoteaddr, &addrlen);
          FD_SET(newfd, &total_list_);  // add to master set
          if (newfd > socket_max_) {    // keep track of the max
            socket_max_ = newfd;
          }
          auto ssl = SSL_new(context_->getContext());
          SSL_set_fd(ssl, newfd);
          auto accept_value = SSL_accept(ssl);
          if (accept_value != -1) {
            logger_->log_trace("Accepted on %d", newfd);
            ssl_map_[newfd] = ssl;
            return newfd;
          } else {
            int ssl_err = SSL_get_error(ssl, accept_value);
            logger_->log_error("Could not accept %d, error code %d", newfd, ssl_err);
            close_ssl(newfd);
            return -1;
          }
        } else {
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
                return -1;
              }
              logger_->log_error("SSL socket connect failed to %s %d", requested_hostname_, port_);
              SSL_free(ssl_);
              ssl_ = NULL;
              close(socket_file_descriptor_);
              return -1;
            } else {
              connected_ = true;
              logger_->log_debug("SSL socket connect success to %s %d, on fd %d", requested_hostname_, port_, socket_file_descriptor_);
              return socket_file_descriptor_;
            }
          }
        }
        return socket_file_descriptor_;
        // we have a new connection
      } else {
        // data to be received on i
        return i;
      }
    }
  }

  bool is_server = listeners_ > 0;

  if (listeners_ == 0) {
    return socket_file_descriptor_;
  }

  logger_->log_trace("%s Could not find a suitable file descriptor or select timed out", is_server ? "Server:" : "Client:");

  return -1;
}

int TLSSocket::writeData(std::vector<uint8_t>& buf, int buflen) {
  int16_t fd = select_descriptor(1000);
  return writeData(buf.data(), buflen, fd);
}

int TLSSocket::readData(std::vector<uint8_t> &buf, int buflen, bool retrieve_all_bytes) {
  if (static_cast<int>(buf.capacity()) < buflen) {
    buf.resize(buflen);
  }
  return readData(reinterpret_cast<uint8_t*>(&buf[0]), buflen, retrieve_all_bytes);
}

int TLSSocket::readData(uint8_t *buf, int buflen, bool retrieve_all_bytes) {
  int total_read = 0;
  int status = 0;
  int loc = 0;
  int16_t fd = select_descriptor(1000);
  auto fd_ssl = get_ssl(fd);
  if (!SSL_pending(fd_ssl)) {
    return 0;
  }
  while (buflen) {
    if (fd <= 0) {
      return -1;
    }
    int sslStatus;
    do {
      status = SSL_read(fd_ssl, buf + loc, buflen);
      sslStatus = SSL_get_error(fd_ssl, status);
    } while (status < 0 && sslStatus == SSL_ERROR_WANT_READ && SSL_pending(fd_ssl));

    buflen -= status;
    loc += status;
    total_read += status;
  }

  return total_read;
}

int TLSSocket::readData(std::vector<uint8_t> &buf, int buflen) {
  if (buf.capacity() < buflen) {
    buf.reserve(buflen);
  }
  int total_read = 0;
  int status = 0;
  int loc = 0;
  while (buflen) {
    int16_t fd = select_descriptor(1000);
    if (fd <= 0) {
      return -1;
    }

    auto fd_ssl = get_ssl(fd);
    int sslStatus;
    do {
      status = SSL_read(fd_ssl, buf.data() + loc, buflen);
      sslStatus = SSL_get_error(fd_ssl, status);
    } while (status < 0 && sslStatus == SSL_ERROR_WANT_READ);

    buflen -= status;
    loc += status;
    total_read += status;
  }

  return total_read;
}

int TLSSocket::writeData(uint8_t *value, int size, int fd) {
  int bytes = 0;
  int sent = 0;
  auto fd_ssl = get_ssl(fd);
  if (IsNullOrEmpty(fd_ssl)) {
    return -1;
  }
  while (bytes < size) {
    sent = SSL_write(fd_ssl, value + bytes, size - bytes);
    // check for errors
    if (sent < 0) {
      int ret = 0;
      ret = SSL_get_error(fd_ssl, sent);
      logger_->log_trace("WriteData socket %d send failed %s %d", fd, strerror(errno), ret);

      return sent;
    }
    logger_->log_trace("WriteData socket %d send succeed %d", fd, sent);
    bytes += sent;
  }
  return size;
}

int TLSSocket::writeData(uint8_t *value, int size) {
  int bytes = 0;
  int sent = 0;
  int fd = select_descriptor(1000);
  auto fd_ssl = get_ssl(fd);
  if (IsNullOrEmpty(fd_ssl))
    return -1;
  while (bytes < size) {
    sent = SSL_write(fd_ssl, value + bytes, size - bytes);
    // check for errors
    if (sent < 0) {
      int ret = 0;
      ret = SSL_get_error(fd_ssl, sent);
      logger_->log_error("WriteData socket %d send failed %s %d", fd, strerror(errno), ret);
      return sent;
    }
    bytes += sent;
  }
  return size;
}

int TLSSocket::readData(uint8_t *buf, int buflen) {
  int total_read = 0;
  int status = 0;
  while (buflen) {
    int16_t fd = select_descriptor(1000);
    if (fd <= 0) {
      close(socket_file_descriptor_);
      return -1;
    }

    int sslStatus;
    do {
      auto fd_ssl = get_ssl(fd);
      if (IsNullOrEmpty(fd_ssl))
        return -1;
      status = SSL_read(fd_ssl, buf, buflen);
      sslStatus = SSL_get_error(fd_ssl, status);
    } while (status < 0 && sslStatus == SSL_ERROR_WANT_READ);

    if (status < 0)
      break;

    buflen -= status;
    buf += status;
    total_read += status;
  }

  return total_read;
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
