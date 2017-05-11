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

TLSContext::TLSContext(const std::shared_ptr<Configure> &configure)
    : SocketContext(configure),
      error_value(0),
      ctx(0),
      logger_(logging::LoggerFactory<TLSContext>::getLogger()),
      configure_(configure) {
}
/**
 * The memory barrier is defined by the singleton
 */
int16_t TLSContext::initialize() {
  if (ctx != 0) {
    return error_value;
  }

  if (nullptr == OpenSSLInitializer::getInstance()) {
    return error_value;
  }

  std::string clientAuthStr;
  bool needClientCert = true;
  if (!(configure_->get(Configure::nifi_security_need_ClientAuth, clientAuthStr)
      && org::apache::nifi::minifi::utils::StringUtils::StringToBool(
          clientAuthStr, needClientCert))) {
    needClientCert = true;
  }

  const SSL_METHOD *method;
  method = TLSv1_2_client_method();
  ctx = SSL_CTX_new(method);
  if (ctx == NULL) {
    logger_->log_error("Could not create SSL context, error: %s.",
                       std::strerror(errno));
    error_value = TLS_ERROR_CONTEXT;
    return error_value;
  }
  if (needClientCert) {
    std::string certificate;
    std::string privatekey;
    std::string passphrase;
    std::string caCertificate;

    if (!(configure_->get(Configure::nifi_security_client_certificate,
                          certificate)
        && configure_->get(Configure::nifi_security_client_private_key,
                           privatekey))) {
      logger_->log_error(
          "Certificate and Private Key PEM file not configured, error: %s.",
          std::strerror(errno));
      error_value = TLS_ERROR_PEM_MISSING;
      return error_value;
    }
    // load certificates and private key in PEM format
    if (SSL_CTX_use_certificate_file(ctx, certificate.c_str(), SSL_FILETYPE_PEM)
        <= 0) {
      logger_->log_error("Could not create load certificate, error : %s",
                         std::strerror(errno));
      error_value = TLS_ERROR_CERT_MISSING;
      return error_value;
    }
    if (configure_->get(Configure::nifi_security_client_pass_phrase,
                        passphrase)) {
      // if the private key has passphase
      SSL_CTX_set_default_passwd_cb(ctx, pemPassWordCb);
      SSL_CTX_set_default_passwd_cb_userdata(
          ctx, static_cast<void*>(configure_.get()));
    }

    int retp = SSL_CTX_use_PrivateKey_file(ctx, privatekey.c_str(),
                                           SSL_FILETYPE_PEM);
    if (retp != 1) {
      logger_->log_error(
          "Could not create load private key,%i on %s error : %s", retp,
          privatekey.c_str(), std::strerror(errno));
      error_value = TLS_ERROR_KEY_ERROR;
      return error_value;
    }
    // verify private key
    if (!SSL_CTX_check_private_key(ctx)) {
      logger_->log_error(
          "Private key does not match the public certificate, error : %s",
          std::strerror(errno));
      error_value = TLS_ERROR_KEY_ERROR;
      return error_value;
    }
    // load CA certificates
    if (configure_->get(Configure::nifi_security_client_ca_certificate,
                        caCertificate)) {
      retp = SSL_CTX_load_verify_locations(ctx, caCertificate.c_str(), 0);
      if (retp == 0) {
        logger_->log_error("Can not load CA certificate, Exiting, error : %s",
                           std::strerror(errno));
        error_value = TLS_ERROR_CERT_ERROR;
        return error_value;
      }
    }

    logger_->log_info("Load/Verify Client Certificate OK.");
  }
  return 0;
}

TLSSocket::~TLSSocket() {
  if (ssl != 0)
    SSL_free(ssl);
}
/**
 * Constructor that accepts host name, port and listeners. With this
 * contructor we will be creating a server socket
 * @param hostname our host name
 * @param port connecting port
 * @param listeners number of listeners in the queue
 */
TLSSocket::TLSSocket(const std::shared_ptr<TLSContext> &context,
                     const std::string &hostname, const uint16_t port,
                     const uint16_t listeners)
    : Socket(context, hostname, port, listeners),
      ssl(0), logger_(logging::LoggerFactory<TLSSocket>::getLogger()) {
  context_ = context;
}

TLSSocket::TLSSocket(const std::shared_ptr<TLSContext> &context,
                     const std::string &hostname, const uint16_t port)
    : Socket(context, hostname, port, 0),
      ssl(0), logger_(logging::LoggerFactory<TLSSocket>::getLogger()) {
  context_ = context;
}

TLSSocket::TLSSocket(const TLSSocket &&d)
    : Socket(std::move(d)),
      ssl(0), logger_(d.logger_) {
  context_ = d.context_;
}

int16_t TLSSocket::initialize() {
  int16_t ret = context_->initialize();
  Socket::initialize();
  if (!ret) {
    // we have s2s secure config
    ssl = SSL_new(context_->getContext());
    SSL_set_fd(ssl, socket_file_descriptor_);
    if (SSL_connect(ssl) == -1) {
      logger_->log_error("SSL socket connect failed to %s %d",
                         requested_hostname_.c_str(), port_);
      SSL_free(ssl);
      ssl = NULL;
      close(socket_file_descriptor_);
      return -1;
    } else {
      logger_->log_info("SSL socket connect success to %s %d",
                        requested_hostname_.c_str(), port_);
      return 0;
    }
  }
  return ret;
}

int16_t TLSSocket::select_descriptor(const uint16_t msec) {
  if (ssl && SSL_pending(ssl))
    return 1;
  return Socket::select_descriptor(msec);
}

int TLSSocket::writeData(std::vector<uint8_t>& buf, int buflen) {
  return Socket::writeData(buf, buflen);
}

int TLSSocket::writeData(uint8_t *value, int size) {
  if (IsNullOrEmpty(ssl))
    return -1;
  // for SSL, wait for the TLS IO is completed
  int bytes = 0;
  int sent = 0;
  while (bytes < size) {
    sent = SSL_write(ssl, value + bytes, size - bytes);
    // check for errors
    if (sent < 0) {
      logger_->log_error("Site2Site Peer socket %d send failed %s",
                         socket_file_descriptor_, strerror(errno));
      return sent;
    }
    bytes += sent;
  }
  return size;
}

int TLSSocket::readData(uint8_t *buf, int buflen) {
  if (IsNullOrEmpty(ssl))
    return -1;
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
      status = SSL_read(ssl, buf, buflen);
      sslStatus = SSL_get_error(ssl, status);
    } while (status < 0 && sslStatus == SSL_ERROR_WANT_READ);

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
