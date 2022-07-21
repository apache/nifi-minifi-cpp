/**
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

#include "controllers/SSLContextService.h"

#ifdef OPENSSL_SUPPORT
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#ifdef WIN32
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "Ws2_32.lib")
#include <optional>
#endif  // WIN32
#endif  // OPENSSL_SUPPORT

#include <fstream>
#include <memory>
#include <string>
#include <set>

#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "io/validation.h"
#include "properties/Configure.h"
#include "utils/gsl.h"
#include "utils/tls/CertificateUtils.h"
#include "utils/tls/TLSUtils.h"
#include "utils/tls/DistinguishedName.h"
#include "utils/tls/WindowsCertStoreLocation.h"
#include "utils/TimeUtil.h"

namespace org::apache::nifi::minifi::controllers {

const core::Property SSLContextService::ClientCertificate(
    core::PropertyBuilder::createProperty("Client Certificate")
        ->withDescription("Client Certificate")
        ->isRequired(false)
        ->build());

const core::Property SSLContextService::PrivateKey(
    core::PropertyBuilder::createProperty("Private Key")
        ->withDescription("Private Key file")
        ->isRequired(false)
        ->build());

const core::Property SSLContextService::Passphrase(
    core::PropertyBuilder::createProperty("Passphrase")
        ->withDescription("Client passphrase. Either a file or unencrypted text")
        ->isRequired(false)
        ->build());

const core::Property SSLContextService::CACertificate(
    core::PropertyBuilder::createProperty("CA Certificate")
        ->withDescription("CA certificate file")
        ->isRequired(false)
        ->build());

const core::Property SSLContextService::UseSystemCertStore(
    core::PropertyBuilder::createProperty("Use System Cert Store")
        ->withDescription("Whether to use the certificates in the OS's certificate store")
        ->isRequired(false)
        ->withDefaultValue<bool>(false)
        ->build());

#ifdef WIN32
const core::Property SSLContextService::CertStoreLocation(
    core::PropertyBuilder::createProperty("Certificate Store Location")
        ->withDescription("One of the Windows certificate store locations, eg. LocalMachine or CurrentUser")
        ->withAllowableValues(utils::tls::WindowsCertStoreLocation::allowedLocations())
        ->isRequired(false)
        ->withDefaultValue(utils::tls::WindowsCertStoreLocation::defaultLocation())
        ->build());

const core::Property SSLContextService::ServerCertStore(
    core::PropertyBuilder::createProperty("Server Cert Store")
        ->withDescription("The name of the certificate store which contains the server certificate")
        ->isRequired(false)
        ->withDefaultValue("ROOT")
        ->build());

const core::Property SSLContextService::ClientCertStore(
    core::PropertyBuilder::createProperty("Client Cert Store")
        ->withDescription("The name of the certificate store which contains the client certificate")
        ->isRequired(false)
        ->withDefaultValue("MY")
        ->build());

const core::Property SSLContextService::ClientCertCN(
    core::PropertyBuilder::createProperty("Client Cert CN")
        ->withDescription("The CN that the client certificate is required to match; default: use the first available client certificate in the store")
        ->isRequired(false)
        ->build());

const core::Property SSLContextService::ClientCertKeyUsage(
    core::PropertyBuilder::createProperty("Client Cert Key Usage")
        ->withDescription("Comma-separated list of enhanced key usage values that the client certificate is required to have")
        ->isRequired(false)
        ->withDefaultValue("Client Authentication")
        ->build());
#endif  // WIN32

void SSLContextService::initialize() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  if (initialized_) {
    return;
  }

  ControllerService::initialize();

  initializeProperties();

  initialized_ = true;
}

#ifdef OPENSSL_SUPPORT
bool SSLContextService::configure_ssl_context(SSL_CTX *ctx) {
  if (!IsNullOrEmpty(certificate_)) {
    if (isFileTypeP12(certificate_)) {
      if (!addP12CertificateToSSLContext(ctx)) {
        return false;
      }
    } else {
      if (!addPemCertificateToSSLContext(ctx)) {
        return false;
      }
    }

    if (!SSL_CTX_check_private_key(ctx)) {
      core::logging::LOG_ERROR(logger_) << "Private key does not match the public certificate, " << getLatestOpenSSLErrorString();
      return false;
    }
  }

  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

  if (!IsNullOrEmpty(ca_certificate_)) {
    if (SSL_CTX_load_verify_locations(ctx, ca_certificate_.c_str(), nullptr) == 0) {
      core::logging::LOG_ERROR(logger_) << "Cannot load CA certificate, exiting, " << getLatestOpenSSLErrorString();
      return false;
    }
  }

  if (use_system_cert_store_ && IsNullOrEmpty(certificate_)) {
    if (!addClientCertificateFromSystemStoreToSSLContext(ctx)) {
      return false;
    }
  }

  if (use_system_cert_store_ && IsNullOrEmpty(ca_certificate_)) {
    if (!addServerCertificatesFromSystemStoreToSSLContext(ctx)) {
      return false;
    }
  }

  return true;
}

bool SSLContextService::addP12CertificateToSSLContext(SSL_CTX* ctx) const {
  auto error = utils::tls::processP12Certificate(certificate_, passphrase_, {
    .cert_cb = [&] (auto cert) -> std::error_code {
      if (SSL_CTX_use_certificate(ctx, cert.get()) != 1) {
        return utils::tls::get_last_ssl_error_code();
      }
      return {};
    },
    .chain_cert_cb = [&] (auto cacert) -> std::error_code {
      if (SSL_CTX_add_extra_chain_cert(ctx, cacert.get()) != 1) {
        return utils::tls::get_last_ssl_error_code();
      }
      static_cast<void>(cacert.release());  // a successful SSL_CTX_add_extra_chain_cert() takes ownership of cacert
      return {};
    },
    .priv_key_cb = [&] (auto priv_key) -> std::error_code {
      if (SSL_CTX_use_PrivateKey(ctx, priv_key.get()) != 1) {
        return utils::tls::get_last_ssl_error_code();
      }
      return {};
    }
  });
  if (error) {
    core::logging::LOG_ERROR(logger_) << error.message();
    return false;
  }
  return true;
}

bool SSLContextService::addPemCertificateToSSLContext(SSL_CTX* ctx) const {
  if (SSL_CTX_use_certificate_chain_file(ctx, certificate_.c_str()) <= 0) {
    core::logging::LOG_ERROR(logger_) << "Could not load client certificate " << certificate_ << ", " << getLatestOpenSSLErrorString();
    return false;
  }

  if (!IsNullOrEmpty(passphrase_)) {
    void* passphrase = const_cast<std::string*>(&passphrase_);
    SSL_CTX_set_default_passwd_cb_userdata(ctx, passphrase);
    SSL_CTX_set_default_passwd_cb(ctx, minifi::utils::tls::pemPassWordCb);
  }

  if (!IsNullOrEmpty(private_key_)) {
    int retp = SSL_CTX_use_PrivateKey_file(ctx, private_key_.c_str(), SSL_FILETYPE_PEM);
    if (retp != 1) {
      core::logging::LOG_ERROR(logger_) << "Could not load private key, " << retp << " on " << private_key_ << ", " << getLatestOpenSSLErrorString();
      return false;
    }
  }

  return true;
}

#ifdef WIN32
bool SSLContextService::findClientCertificate(ClientCertCallback cb) const {
  utils::tls::WindowsCertStore cert_store(utils::tls::WindowsCertStoreLocation{cert_store_location_}, client_cert_store_);
  if (auto error = cert_store.error()) {
    logger_->log_error("Could not open system certificate store %s/%s (client certificates): %s", cert_store_location_, client_cert_store_, error.message());
    return false;
  }

  logger_->log_debug("Looking for client certificate in sytem store %s/%s", cert_store_location_, client_cert_store_);

  while (auto cert_ctx = cert_store.nextCert()) {
    if (useClientCertificate(cert_ctx, cb)) {
      return true;
    }
  }

  logger_->log_error("Could not find any suitable client certificate in sytem store %s/%s", cert_store_location_, client_cert_store_);
  return false;
}

#endif

#ifdef WIN32
bool SSLContextService::addClientCertificateFromSystemStoreToSSLContext(SSL_CTX* ctx) const {
  return findClientCertificate([&] (auto cert, auto priv_key) -> bool {
    if (SSL_CTX_use_certificate(ctx, cert.get()) != 1) {
      logger_->log_error("Failed to set certificate from %s, %s", cert->name, getLatestOpenSSLErrorString);
      return false;
    }

    if (SSL_CTX_use_PrivateKey(ctx, priv_key.get()) != 1) {
      logger_->log_error("Failed to use private key %s, %s", cert->name, getLatestOpenSSLErrorString());
      return false;
    }
    return true;
  });
}
#else
bool SSLContextService::addClientCertificateFromSystemStoreToSSLContext(SSL_CTX* /*ctx*/) const {
  logger_->log_error("Getting client certificate from the system store is only supported on Windows");
  return false;
}
#endif  // WIN32

#ifdef WIN32
bool SSLContextService::useClientCertificate(PCCERT_CONTEXT certificate, ClientCertCallback cb) const {
  utils::tls::X509_unique_ptr x509_cert = utils::tls::convertWindowsCertificate(certificate);
  if (!x509_cert) {
    logger_->log_error("Failed to convert system store client certificate to X.509 format");
    return false;
  }

  utils::tls::EVP_PKEY_unique_ptr private_key = utils::tls::extractPrivateKey(certificate);
  if (!private_key) {
    logger_->log_debug("Skipping client certificate %s because it has no exportable private key", x509_cert->name);
    return false;
  }

  if (!client_cert_cn_.empty()) {
    utils::tls::DistinguishedName dn = utils::tls::DistinguishedName::fromSlashSeparated(x509_cert->name);
    std::optional<std::string> cn = dn.getCN();
    if (!cn || *cn != client_cert_cn_) {
      logger_->log_debug("Skipping client certificate %s because it doesn't match CN=%s", x509_cert->name, client_cert_cn_);
      return false;
    }
  }

  utils::tls::EXTENDED_KEY_USAGE_unique_ptr key_usage{static_cast<EXTENDED_KEY_USAGE*>(X509_get_ext_d2i(x509_cert.get(), NID_ext_key_usage, nullptr, nullptr))};
  if (!key_usage) {
    logger_->log_error("Skipping client certificate %s because it has no extended key usage", x509_cert->name);
    return false;
  }

  if (!(client_cert_key_usage_.isSubsetOf(utils::tls::ExtendedKeyUsage{*key_usage}))) {
    logger_->log_debug("Skipping client certificate %s because its extended key usage set does not contain all usages specified in %s",
                       x509_cert->name, Configuration::nifi_security_windows_client_cert_key_usage);
    return false;
  }

  std::string cert_name = x509_cert->name;
  if (cb(std::move(x509_cert), std::move(private_key))) {
    logger_->log_debug("Found client certificate %s", cert_name);
    return true;
  }

  return false;
}
#endif  // WIN32

bool SSLContextService::addServerCertificatesFromSystemStoreToSSLContext(SSL_CTX* ctx) const {  // NOLINT(readability-convert-member-functions-to-static)
#ifdef WIN32
  X509_STORE* ssl_store = SSL_CTX_get_cert_store(ctx);
  if (!ssl_store) {
    logger_->log_error("Could not get handle to SSL certificate store");
    return false;
  }

  findServerCertificate([&] (auto cert) -> bool {
    // return false to indicate that we wish to iterate over all subsequent certificates as well
    int success = X509_STORE_add_cert(ssl_store, cert.get());
    if (success == 1) {
      logger_->log_debug("Added server certificate %s from the system store to the SSL store", cert->name);
      return false;
    }

    auto err = ERR_peek_last_error();
    if (ERR_GET_REASON(err) == X509_R_CERT_ALREADY_IN_HASH_TABLE) {
      logger_->log_debug("Ignoring duplicate server certificate %s", cert->name);
      return false;
    }

    logger_->log_error("Failed to add server certificate %s to the SSL store; error: %s", cert->name, getLatestOpenSSLErrorString());
    return false;
  });

  return true;
#else
  SSL_CTX_set_default_verify_paths(ctx);
  return true;
#endif  // WIN32
}

#ifdef WIN32
bool SSLContextService::findServerCertificate(ServerCertCallback cb) const {
  utils::tls::WindowsCertStore cert_store(utils::tls::WindowsCertStoreLocation{cert_store_location_}, server_cert_store_);
  if (auto error = cert_store.error()) {
    logger_->log_error("Could not open system certificate store %s/%s (server certificates): %s", cert_store_location_, server_cert_store_, error.message());
    return false;
  }

  logger_->log_debug("Adding server certificates from system store %s/%s", cert_store_location_, server_cert_store_);

  while (auto cert_ctx = cert_store.nextCert()) {
    if (useServerCertificate(cert_ctx, cb)) {
      return true;
    }
  }

  return false;
}
#endif

#ifdef WIN32
bool SSLContextService::useServerCertificate(PCCERT_CONTEXT certificate, ServerCertCallback cb) const {
  utils::tls::X509_unique_ptr x509_cert = utils::tls::convertWindowsCertificate(certificate);
  if (!x509_cert) {
    logger_->log_error("Failed to convert system store server certificate to X.509 format");
    return false;
  }

  return cb(std::move(x509_cert));
}
#endif  // WIN32
#endif  // OPENSSL_SUPPORT

/**
 * If OpenSSL is not installed we may still continue operations. Nullptr will
 * be returned and it will be up to the caller to determine if this failure is
 * recoverable.
 */
std::unique_ptr<SSLContext> SSLContextService::createSSLContext() {
#ifdef OPENSSL_SUPPORT
  SSL_library_init();
  const SSL_METHOD *method;

  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  method = TLSv1_2_client_method();
  SSL_CTX *ctx = SSL_CTX_new(method);

  if (ctx == nullptr) {
    return nullptr;
  }

  if (!configure_ssl_context(ctx)) {
    SSL_CTX_free(ctx);
    return nullptr;
  }

  return std::make_unique<SSLContext>(ctx);
#else
  return nullptr;
#endif
}

const std::string &SSLContextService::getCertificateFile() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return certificate_;
}

const std::string &SSLContextService::getPassphrase() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return passphrase_;
}

const std::string &SSLContextService::getPassphraseFile() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return passphrase_file_;
}

const std::string &SSLContextService::getPrivateKeyFile() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return private_key_;
}

const std::string &SSLContextService::getCACertificate() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return ca_certificate_;
}

void SSLContextService::onEnable() {
  valid_ = true;
  std::string default_dir;

  if (nullptr != configuration_)
    configuration_->get(Configure::nifi_default_directory, default_dir);

  logger_->log_trace("onEnable()");

  bool has_certificate_property = getProperty(ClientCertificate.getName(), certificate_);
  if (has_certificate_property) {
    std::ifstream cert_file(certificate_);
    if (!cert_file.good()) {
      logger_->log_warn("Cannot open certificate file %s", certificate_);
      std::string test_cert = default_dir + certificate_;
      std::ifstream cert_file_test(test_cert);
      if (cert_file_test.good()) {
        certificate_ = test_cert;
        logger_->log_info("Using certificate file %s", certificate_);
      } else {
        logger_->log_error("Cannot open certificate file %s", test_cert);
        valid_ = false;
      }
      cert_file_test.close();
    }
    cert_file.close();
  } else {
    logger_->log_debug("Certificate empty");
  }

  if (has_certificate_property && !isFileTypeP12(certificate_)) {
    if (getProperty(PrivateKey.getName(), private_key_)) {
      std::ifstream priv_file(private_key_);
      if (!priv_file.good()) {
        logger_->log_warn("Cannot open private key file %s", private_key_);
        std::string test_priv = default_dir + private_key_;
        std::ifstream private_file_test(test_priv);
        if (private_file_test.good()) {
          private_key_ = test_priv;
          logger_->log_info("Using private key file %s", private_key_);
        } else {
          logger_->log_error("Cannot open private key file %s", test_priv);
          valid_ = false;
        }
        private_file_test.close();
      }
      priv_file.close();
    } else {
      logger_->log_debug("Private key empty");
    }
  }

  if (!getProperty(Passphrase.getName(), passphrase_)) {
    logger_->log_debug("No pass phrase for %s", certificate_);
  } else {
    std::ifstream passphrase_file(passphrase_);
    if (passphrase_file.good()) {
      passphrase_file_ = passphrase_;
      // we should read it from the file
      passphrase_.assign((std::istreambuf_iterator<char>(passphrase_file)), std::istreambuf_iterator<char>());
    } else {
      std::string test_passphrase = default_dir + passphrase_;
      std::ifstream passphrase_file_test(test_passphrase);
      if (passphrase_file_test.good()) {
        passphrase_ = test_passphrase;
        passphrase_file_ = test_passphrase;
        passphrase_.assign((std::istreambuf_iterator<char>(passphrase_file_test)), std::istreambuf_iterator<char>());
      } else {
        // not an invalid file since we support a passphrase of unencrypted text
      }
      passphrase_file_test.close();
    }
    passphrase_file.close();
  }

  if (getProperty(CACertificate.getName(), ca_certificate_)) {
    std::ifstream cert_file(ca_certificate_);
    if (!cert_file.good()) {
      std::string test_ca_cert = default_dir + ca_certificate_;
      std::ifstream ca_cert_file_file_test(test_ca_cert);
      if (ca_cert_file_file_test.good()) {
        ca_certificate_ = test_ca_cert;
      } else {
        valid_ = false;
      }
      ca_cert_file_file_test.close();
    }
    cert_file.close();
  }

  getProperty(UseSystemCertStore.getName(), use_system_cert_store_);

#ifdef WIN32
  getProperty(CertStoreLocation.getName(), cert_store_location_);
  getProperty(ServerCertStore.getName(), server_cert_store_);
  getProperty(ClientCertStore.getName(), client_cert_store_);
  getProperty(ClientCertCN.getName(), client_cert_cn_);

  std::string client_cert_key_usage;
  getProperty(ClientCertKeyUsage.getName(), client_cert_key_usage);
  client_cert_key_usage_ = utils::tls::ExtendedKeyUsage{client_cert_key_usage};
#endif  // WIN32

  verifyCertificateExpiration();
}

void SSLContextService::initializeProperties() {
  setSupportedProperties(properties());
}

void SSLContextService::verifyCertificateExpiration() {
  auto verify = [&] (const std::string& cert_file, const utils::tls::X509_unique_ptr& cert) {
    if (auto end_date = utils::tls::getCertificateExpiration(cert)) {
      std::string end_date_str = utils::timeutils::getTimeStr(*end_date);
      if (end_date.value() < std::chrono::system_clock::now()) {
        core::logging::LOG_ERROR(logger_) << "Certificate in '" << cert_file << "' expired at " << end_date_str;
      } else if (auto diff = end_date.value() - std::chrono::system_clock::now(); diff < std::chrono::weeks{2}) {
        core::logging::LOG_WARN(logger_) << "Certificate in '" << cert_file << "' will expire at " << end_date_str;
      } else {
        core::logging::LOG_DEBUG(logger_) << "Certificate in '" << cert_file << "' will expire at " << end_date_str;
      }
    } else {
      core::logging::LOG_ERROR(logger_) << "Could not determine expiration date for certificate in '" << cert_file << "'";
    }
  };
  if (!IsNullOrEmpty(certificate_)) {
    if (isFileTypeP12(certificate_)) {
      auto error = utils::tls::processP12Certificate(certificate_, passphrase_, {
          .cert_cb = [&](auto cert) -> std::error_code {
            verify(certificate_, cert);
            return {};
          },
          .chain_cert_cb = [&](auto cert) -> std::error_code {
            verify(certificate_, cert);
            return {};
          },
          .priv_key_cb = {}
      });
      if (error) {
        core::logging::LOG_ERROR(logger_) << error.value();
      }
    } else {
      auto error = utils::tls::processPEMCertificate(certificate_, passphrase_, {
          .cert_cb = [&](auto cert) -> std::error_code {
            verify(certificate_, cert);
            return {};
          },
          .chain_cert_cb = [&](auto cert) -> std::error_code {
            verify(certificate_, cert);
            return {};
          },
          .priv_key_cb = {}
      });
      if (error) {
        core::logging::LOG_ERROR(logger_) << error.value();
      }
    }
  }

  if (!IsNullOrEmpty(ca_certificate_)) {
    auto error = utils::tls::processPEMCertificate(ca_certificate_, std::nullopt, {
        .cert_cb = [&](auto cert) -> std::error_code {
          verify(ca_certificate_, cert);
          return {};
        },
        .chain_cert_cb = [&](auto cert) -> std::error_code {
          verify(ca_certificate_, cert);
          return {};
        },
        .priv_key_cb = {}
    });
    if (error) {
      core::logging::LOG_ERROR(logger_) << error.message();
    }
  }

#ifdef WIN32
  if (use_system_cert_store_ && IsNullOrEmpty(certificate_)) {
    findClientCertificate([&] (auto cert, auto /*priv_key*/) -> bool {
      verify(cert->name, cert);
      return false;  // keep on iterating, check all
    });
  }

  if (use_system_cert_store_ && IsNullOrEmpty(ca_certificate_)) {
    findServerCertificate([&] (auto cert) -> bool {
      verify(cert->name, cert);
      return false;  // keep on iterating, check all
    });
  }
#endif
}

REGISTER_RESOURCE(SSLContextService, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
