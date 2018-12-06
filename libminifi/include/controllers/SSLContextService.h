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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_SSLCONTEXTSERVICE_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_SSLCONTEXTSERVICE_H_
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif 
#ifdef OPENSSL_SUPPORT
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif
#include <iostream>
#include <memory>
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "io/validation.h"
#include "../core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

class SSLContext {
 public:
#ifdef OPENSSL_SUPPORT
  SSLContext(SSL_CTX *context)
      : context_(context) {

  }
#else
	 SSLContext(void *context) {

	 }
#endif
  ~SSLContext() {
#ifdef OPENSSL_SUPPORT
    if (context_) {
      SSL_CTX_free(context_);
    }
#endif
  }
 protected:
#ifdef OPENSSL_SUPPORT
  SSL_CTX *context_;
#endif
};

/**
 * SSLContextService provides a configurable controller service from
 * which we can provide an SSL Context or component parts that go
 * into creating one.
 *
 * Justification: Abstracts SSL support out of processors into a
 * configurable controller service.
 */
class SSLContextService : public core::controller::ControllerService {
 public:
  explicit SSLContextService(const std::string &name, const std::string &id)
      : ControllerService(name, id),
        initialized_(false),
        valid_(false),
        logger_(logging::LoggerFactory<SSLContextService>::getLogger()) {
  }

  explicit SSLContextService(const std::string &name, utils::Identifier uuid = utils::Identifier())
      : ControllerService(name, uuid),
        initialized_(false),
        valid_(false),
        logger_(logging::LoggerFactory<SSLContextService>::getLogger()) {
  }

  explicit SSLContextService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : ControllerService(name),
        initialized_(false),
        valid_(false),
        logger_(logging::LoggerFactory<SSLContextService>::getLogger()) {
    setConfiguration(configuration);
    initialize();
    // set the properties based on the configuration
    core::Property property("Client Certificate", "Client Certificate");
    core::Property privKey("Private Key", "Private Key file");
    core::Property passphrase_prop("Passphrase", "Client passphrase. Either a file or unencrypted text");
    core::Property caCert("CA Certificate", "CA certificate file");

    std::string value;
    if (configuration_->get(Configure::nifi_security_client_certificate, value)) {
      setProperty(property.getName(), value);
    }

    if (configuration_->get(Configure::nifi_security_client_private_key, value)) {
      setProperty(privKey.getName(), value);
    }

    if (configuration_->get(Configure::nifi_security_client_pass_phrase, value)) {
      setProperty(passphrase_prop.getName(), value);
    }

    if (configuration_->get(Configure::nifi_security_client_ca_certificate, value)) {
      setProperty(caCert.getName(), value);
    }
  }

  virtual void initialize();

  std::unique_ptr<SSLContext> createSSLContext();

  const std::string &getCertificateFile();

  const std::string &getPassphrase();

  const std::string &getPassphraseFile();

  const std::string &getPrivateKeyFile();

  const std::string &getCACertificate();

  void yield() {

  }

  bool isRunning() {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() {
    return false;
  }

#ifdef OPENSSL_SUPPORT
  bool configure_ssl_context(SSL_CTX *ctx) {
    if (!IsNullOrEmpty(certificate)) {
      if (SSL_CTX_use_certificate_file(ctx, certificate.c_str(), SSL_FILETYPE_PEM) <= 0) {
        logger_->log_error("Could not create load certificate, error : %s", std::strerror(errno));
        return false;
      }
      if (!IsNullOrEmpty(passphrase_)) {
        SSL_CTX_set_default_passwd_cb_userdata(ctx, &passphrase_);
        SSL_CTX_set_default_passwd_cb(ctx, pemPassWordCb);
      }
    }

    if (!IsNullOrEmpty(private_key_)) {
      int retp = SSL_CTX_use_PrivateKey_file(ctx, private_key_.c_str(), SSL_FILETYPE_PEM);
      if (retp != 1) {
        logger_->log_error("Could not create load private key,%i on %s error : %s", retp, private_key_, std::strerror(errno));
        return false;
      }

      if (!SSL_CTX_check_private_key(ctx)) {
        logger_->log_error("Private key does not match the public certificate, error : %s", std::strerror(errno));
        return false;
      }
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    int retp = SSL_CTX_load_verify_locations(ctx, ca_certificate_.c_str(), 0);

    if (retp == 0) {
      logger_->log_error("Can not load CA certificate, Exiting, error : %s", std::strerror(errno));
      return false;
    }

    return true;
  }
#endif

  virtual void onEnable();

 protected:

  static int pemPassWordCb(char *buf, int size, int rwflag, void *userdata) {

    std::string *pass = (std::string*) userdata;
    if (pass->length() > 0) {

      memset(buf, 0x00, size);
      memcpy(buf, pass->c_str(), pass->length() - 1);

      return pass->length() - 1;
    }
    return 0;
  }

  virtual void initializeTLS();

  std::mutex initialization_mutex_;
  std::atomic<bool> initialized_;
  std::atomic<bool> valid_;
  std::string certificate;
  std::string private_key_;
  std::string passphrase_;
  std::string passphrase_file_;
  std::string ca_certificate_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};
typedef int (SSLContextService::*ptr)(char *, int, int, void *);
REGISTER_RESOURCE(SSLContextService, "Controller service that provides SSL/TLS capabilities to consuming interfaces");

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_CONTROLLERS_SSLCONTEXTSERVICE_H_ */
