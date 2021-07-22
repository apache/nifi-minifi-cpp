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

#ifdef WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

#ifdef OPENSSL_SUPPORT
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/pkcs12.h>
#endif
#include <iostream>
#include <memory>
#include <string>
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "utils/tls/ExtendedKeyUsage.h"
#include "io/validation.h"
#include "../core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

class SSLContext {
 public:
#ifdef OPENSSL_SUPPORT
  SSLContext(SSL_CTX *context) // NOLINT
      : context_(context) {
  }
#else
  SSLContext(void *context) {} // NOLINT
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
  explicit SSLContextService(const std::string &name, const utils::Identifier &uuid = {})
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
    std::string value;
    if (configuration_->get(Configure::nifi_security_client_certificate, value)) {
      setProperty(ClientCertificate.getName(), value);
    }

    if (configuration_->get(Configure::nifi_security_client_private_key, value)) {
      setProperty(PrivateKey.getName(), value);
    }

    if (configuration_->get(Configure::nifi_security_client_pass_phrase, value)) {
      setProperty(Passphrase.getName(), value);
    }

    if (configuration_->get(Configure::nifi_security_client_ca_certificate, value)) {
      setProperty(CACertificate.getName(), value);
    }

    if (configuration_->get(Configure::nifi_security_use_system_cert_store, value)) {
      setProperty(UseSystemCertStore.getName(), value);
    }

#ifdef WIN32
    if (configuration_->get(Configure::nifi_security_windows_cert_store_location, value)) {
      setProperty(CertStoreLocation.getName(), value);
    }

    if (configuration_->get(Configure::nifi_security_windows_server_cert_store, value)) {
      setProperty(ServerCertStore.getName(), value);
    }

    if (configuration_->get(Configure::nifi_security_windows_client_cert_store, value)) {
      setProperty(ClientCertStore.getName(), value);
    }

    if (configuration_->get(Configure::nifi_security_windows_client_cert_cn, value)) {
      setProperty(ClientCertCN.getName(), value);
    }

    if (configuration_->get(Configure::nifi_security_windows_client_cert_key_usage, value)) {
      setProperty(ClientCertKeyUsage.getName(), value);
    }
#endif  // WIN32
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
  bool configure_ssl_context(SSL_CTX *ctx);
#endif

  virtual void onEnable();

  MINIFIAPI static const core::Property ClientCertificate;
  MINIFIAPI static const core::Property PrivateKey;
  MINIFIAPI static const core::Property Passphrase;
  MINIFIAPI static const core::Property CACertificate;
  MINIFIAPI static const core::Property UseSystemCertStore;
#ifdef WIN32
  MINIFIAPI static const core::Property CertStoreLocation;
  MINIFIAPI static const core::Property ServerCertStore;
  MINIFIAPI static const core::Property ClientCertStore;
  MINIFIAPI static const core::Property ClientCertCN;
  MINIFIAPI static const core::Property ClientCertKeyUsage;
#endif  // WIN32

 protected:
  virtual void initializeProperties();

  std::mutex initialization_mutex_;
  std::atomic<bool> initialized_;
  std::atomic<bool> valid_;
  std::string certificate_;
  std::string private_key_;
  std::string passphrase_;
  std::string passphrase_file_;
  std::string ca_certificate_;
  bool use_system_cert_store_ = false;
#ifdef WIN32
  std::string cert_store_location_;
  std::string server_cert_store_;
  std::string client_cert_store_;
  std::string client_cert_cn_;
  utils::tls::ExtendedKeyUsage client_cert_key_usage_;
#endif  // WIN32

#ifdef OPENSSL_SUPPORT
  static std::string getLatestOpenSSLErrorString() {
    unsigned long err = ERR_peek_last_error(); // NOLINT
    if (err == 0U) {
      return "";
    }
    char buf[4096];
    ERR_error_string_n(err, buf, sizeof(buf));
    return buf;
  }
#endif

  static bool isFileTypeP12(const std::string& filename) {
    return utils::StringUtils::endsWithIgnoreCase(filename, "p12");
  }

 private:
#ifdef OPENSSL_SUPPORT
  bool addP12CertificateToSSLContext(SSL_CTX* ctx) const;
  bool addPemCertificateToSSLContext(SSL_CTX* ctx) const;
  bool addClientCertificateFromSystemStoreToSSLContext(SSL_CTX* ctx) const;
  bool addServerCertificatesFromSystemStoreToSSLContext(SSL_CTX* ctx) const;
#ifdef WIN32
  bool useClientCertificate(SSL_CTX* ctx, PCCERT_CONTEXT certificate) const;
  void addServerCertificateToSSLStore(X509_STORE* ssl_store, PCCERT_CONTEXT certificate) const;
#endif  // WIN32
#endif  // OPENSSL_SUPPORT

  std::shared_ptr<logging::Logger> logger_;
};
typedef int (SSLContextService::*ptr)(char *, int, int, void *);

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_CONTROLLERS_SSLCONTEXTSERVICE_H_
