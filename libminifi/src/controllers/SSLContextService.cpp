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

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/bio.h>
#ifdef WIN32
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "Ws2_32.lib")
#include <optional>
#endif  // WIN32

#include <fstream>
#include <memory>
#include <string>

#include "core/Resource.h"
#include "io/validation.h"
#include "properties/Configure.h"
#include "utils/HTTPUtils.h"
#include "utils/tls/CertificateUtils.h"
#include "utils/tls/TLSUtils.h"
#include "utils/tls/DistinguishedName.h"
#include "utils/tls/WindowsCertStoreLocation.h"
#include "utils/TimeUtil.h"

namespace org::apache::nifi::minifi::controllers {

namespace {
bool is_valid_and_readable_path(const std::filesystem::path& path_to_be_tested) {
  std::ifstream file_to_be_tested(path_to_be_tested);
  return file_to_be_tested.good();
}

#ifdef WIN32
std::string getCertName(const utils::tls::X509_unique_ptr& cert) {
  const size_t BUFFER_SIZE = 256;
  char name_buffer[BUFFER_SIZE];

  BIO* bio = BIO_new(BIO_s_mem());
  if (!bio) {
    return {};
  }
  const auto guard = gsl::finally([&bio]() {
    BIO_free(bio);
  });

  X509_NAME_print_ex(bio, X509_get_subject_name(cert.get()), 0, XN_FLAG_ONELINE);

  int length = BIO_read(bio, name_buffer, BUFFER_SIZE - 1);
  name_buffer[length] = '\0';

  return name_buffer;
}
#endif
}  // namespace

void SSLContextServiceImpl::initialize() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  if (initialized_) {
    return;
  }

  ControllerServiceImpl::initialize();

  initializeProperties();

  initialized_ = true;
}

bool SSLContextServiceImpl::configure_ssl_context(void* raw_ctx) {
  auto* const ctx = static_cast<SSL_CTX*>(raw_ctx);
  if (!certificate_.empty()) {
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
      logger_->log_error("Private key does not match the public certificate, {}", getLatestOpenSSLErrorString());
      return false;
    }
  }

  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

  if (!ca_certificate_.empty()) {
    if (SSL_CTX_load_verify_locations(ctx, ca_certificate_.string().c_str(), nullptr) == 0) {
      logger_->log_error("Cannot load CA certificate, exiting, {}", getLatestOpenSSLErrorString());
      return false;
    }
  }

  if (use_system_cert_store_ && certificate_.empty()) {
    if (!addClientCertificateFromSystemStoreToSSLContext(ctx)) {
      return false;
    }
  }

  if (use_system_cert_store_ && ca_certificate_.empty()) {
    if (!addServerCertificatesFromSystemStoreToSSLContext(ctx)) {
      return false;
    }
  }

  // Security level set to 0 for backwards compatibility to support TLS versions below v1.2
  if ((minimum_tls_version_ != -1 && minimum_tls_version_ < TLS1_2_VERSION) || (maximum_tls_version_ != -1 && maximum_tls_version_ < TLS1_2_VERSION)) {
    SSL_CTX_set_security_level(ctx, 0);
  }

  if (minimum_tls_version_ != -1) {
    SSL_CTX_set_min_proto_version(ctx, minimum_tls_version_);
  }

  if (maximum_tls_version_ != -1) {
    SSL_CTX_set_max_proto_version(ctx, maximum_tls_version_);
  }

  return true;
}

bool SSLContextServiceImpl::addP12CertificateToSSLContext(SSL_CTX* ctx) const {
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
    logger_->log_error("{}", error.message());
    return false;
  }
  return true;
}

bool SSLContextServiceImpl::addPemCertificateToSSLContext(SSL_CTX* ctx) const {
  if (SSL_CTX_use_certificate_chain_file(ctx, certificate_.string().c_str()) <= 0) {
    logger_->log_error("Could not load client certificate {}, {}", certificate_.string(), getLatestOpenSSLErrorString());
    return false;
  }

  if (!IsNullOrEmpty(passphrase_)) {
    void* passphrase = const_cast<std::string*>(&passphrase_);
    SSL_CTX_set_default_passwd_cb_userdata(ctx, passphrase);
    SSL_CTX_set_default_passwd_cb(ctx, minifi::utils::tls::pemPassWordCb);
  }

  if (!IsNullOrEmpty(private_key_)) {
    int retp = SSL_CTX_use_PrivateKey_file(ctx, private_key_.string().c_str(), SSL_FILETYPE_PEM);
    if (retp != 1) {
      logger_->log_error("Could not load private key, {} on {}, {}", retp, private_key_, getLatestOpenSSLErrorString());
      return false;
    }
  }

  return true;
}

#ifdef WIN32
bool SSLContextServiceImpl::findClientCertificate(ClientCertCallback cb) const {
  utils::tls::WindowsCertStore cert_store(utils::tls::WindowsCertStoreLocation{cert_store_location_}, client_cert_store_);
  if (auto error = cert_store.error()) {
    logger_->log_error("Could not open system certificate store {}/{} (client certificates): {}", cert_store_location_, client_cert_store_, error.message());
    return false;
  }

  logger_->log_debug("Looking for client certificate in sytem store {}/{}", cert_store_location_, client_cert_store_);

  while (auto cert_ctx = cert_store.nextCert()) {
    if (useClientCertificate(cert_ctx, cb)) {
      return true;
    }
  }

  logger_->log_error("Could not find any suitable client certificate in sytem store {}/{}", cert_store_location_, client_cert_store_);
  return false;
}

#endif

#ifdef WIN32
bool SSLContextServiceImpl::addClientCertificateFromSystemStoreToSSLContext(SSL_CTX* ctx) const {
  return findClientCertificate([&] (auto cert, auto priv_key) -> bool {
    auto cert_name = getCertName(cert);
    if (SSL_CTX_use_certificate(ctx, cert.get()) != 1) {
      logger_->log_error("Failed to set certificate from {}, {}", cert_name, getLatestOpenSSLErrorString());
      return false;
    }

    if (SSL_CTX_use_PrivateKey(ctx, priv_key.get()) != 1) {
      logger_->log_error("Failed to use private key {}, {}", cert_name, getLatestOpenSSLErrorString());
      return false;
    }
    return true;
  });
}
#else
bool SSLContextServiceImpl::addClientCertificateFromSystemStoreToSSLContext(SSL_CTX* /*ctx*/) const {
  logger_->log_error("Getting client certificate from the system store is only supported on Windows");
  return false;
}
#endif  // WIN32

#ifdef WIN32
bool SSLContextServiceImpl::useClientCertificate(PCCERT_CONTEXT certificate, ClientCertCallback cb) const {
  utils::tls::X509_unique_ptr x509_cert = utils::tls::convertWindowsCertificate(certificate);
  if (!x509_cert) {
    logger_->log_error("Failed to convert system store client certificate to X.509 format");
    return false;
  }

  std::string x509_name = getCertName(x509_cert);
  utils::tls::EVP_PKEY_unique_ptr private_key = utils::tls::extractPrivateKey(certificate);
  if (!private_key) {
    logger_->log_debug("Skipping client certificate {} because it has no exportable private key", x509_name);
    return false;
  }

  if (!client_cert_cn_.empty()) {
    utils::tls::DistinguishedName dn = utils::tls::DistinguishedName::fromSlashSeparated(x509_name);
    std::optional<std::string> cn = dn.getCN();
    if (!cn || *cn != client_cert_cn_) {
      logger_->log_debug("Skipping client certificate {} because it doesn't match CN={}", x509_name, client_cert_cn_);
      return false;
    }
  }

  utils::tls::EXTENDED_KEY_USAGE_unique_ptr key_usage{static_cast<EXTENDED_KEY_USAGE*>(X509_get_ext_d2i(x509_cert.get(), NID_ext_key_usage, nullptr, nullptr))};
  if (!key_usage) {
    logger_->log_error("Skipping client certificate {} because it has no extended key usage", x509_name);
    return false;
  }

  if (!(client_cert_key_usage_.isSubsetOf(utils::tls::ExtendedKeyUsage{*key_usage}))) {
    logger_->log_debug("Skipping client certificate {} because its extended key usage set does not contain all usages specified in {}",
                       x509_name, Configuration::nifi_security_windows_client_cert_key_usage);
    return false;
  }

  if (cb(std::move(x509_cert), std::move(private_key))) {
    logger_->log_debug("Found client certificate {}", x509_name);
    return true;
  }

  return false;
}
#endif  // WIN32

bool SSLContextServiceImpl::addServerCertificatesFromSystemStoreToSSLContext(SSL_CTX* ctx) const {  // NOLINT(readability-convert-member-functions-to-static)
#ifdef WIN32
  X509_STORE* ssl_store = SSL_CTX_get_cert_store(ctx);
  if (!ssl_store) {
    logger_->log_error("Could not get handle to SSL certificate store");
    return false;
  }

  findServerCertificate([&] (auto cert) -> bool {
    // return false to indicate that we wish to iterate over all subsequent certificates as well
    auto cert_name = getCertName(cert);
    int success = X509_STORE_add_cert(ssl_store, cert.get());
    if (success == 1) {
      logger_->log_debug("Added server certificate {} from the system store to the SSL store", cert_name);
      return false;
    }

    auto err = ERR_peek_last_error();
    if (ERR_GET_REASON(err) == X509_R_CERT_ALREADY_IN_HASH_TABLE) {
      logger_->log_debug("Ignoring duplicate server certificate {}", cert_name);
      return false;
    }

    logger_->log_error("Failed to add server certificate {} to the SSL store; error: {}", cert_name, getLatestOpenSSLErrorString());
    return false;
  });

  return true;
#else
  static const auto default_ca_file = utils::getDefaultCAFile();
  if (default_ca_file) {
    SSL_CTX_load_verify_file(ctx, std::string(*default_ca_file).c_str());
  } else {
    SSL_CTX_set_default_verify_paths(ctx);
  }
  return true;
#endif  // WIN32
}

#ifdef WIN32
bool SSLContextServiceImpl::findServerCertificate(ServerCertCallback cb) const {
  utils::tls::WindowsCertStore cert_store(utils::tls::WindowsCertStoreLocation{cert_store_location_}, server_cert_store_);
  if (auto error = cert_store.error()) {
    logger_->log_error("Could not open system certificate store {}/{} (server certificates): {}", cert_store_location_, server_cert_store_, error.message());
    return false;
  }

  logger_->log_debug("Adding server certificates from system store {}/{}", cert_store_location_, server_cert_store_);

  while (auto cert_ctx = cert_store.nextCert()) {
    if (useServerCertificate(cert_ctx, cb)) {
      return true;
    }
  }

  return false;
}
#endif

#ifdef WIN32
bool SSLContextServiceImpl::useServerCertificate(PCCERT_CONTEXT certificate, ServerCertCallback cb) const {
  utils::tls::X509_unique_ptr x509_cert = utils::tls::convertWindowsCertificate(certificate);
  if (!x509_cert) {
    logger_->log_error("Failed to convert system store server certificate to X.509 format");
    return false;
  }

  return cb(std::move(x509_cert));
}
#endif  // WIN32

/**
 * If OpenSSL is not installed we may still continue operations. Nullptr will
 * be returned and it will be up to the caller to determine if this failure is
 * recoverable.
 */
std::unique_ptr<SSLContext> SSLContextServiceImpl::createSSLContext() {
  SSL_library_init();
  const SSL_METHOD *method = nullptr;

  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  method = TLS_client_method();
  SSL_CTX *ctx = SSL_CTX_new(method);

  if (ctx == nullptr) {
    return nullptr;
  }

  if (!configure_ssl_context(ctx)) {
    SSL_CTX_free(ctx);
    return nullptr;
  }

  return std::make_unique<SSLContext>(ctx);
}

const std::filesystem::path &SSLContextServiceImpl::getCertificateFile() const {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return certificate_;
}

const std::string &SSLContextServiceImpl::getPassphrase() const {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return passphrase_;
}

const std::filesystem::path &SSLContextServiceImpl::getPrivateKeyFile() const {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return private_key_;
}

const std::filesystem::path &SSLContextServiceImpl::getCACertificate() const {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return ca_certificate_;
}

void SSLContextServiceImpl::onEnable() {
  std::filesystem::path default_dir;

  if (configuration_) {
    if (auto default_dir_str = configuration_->get(Configure::nifi_default_directory)) {
      default_dir = default_dir_str.value();
    }
  }

  logger_->log_trace("onEnable()");

  certificate_.clear();
  if (auto certificate = getProperty(ClientCertificate)) {
    if (is_valid_and_readable_path(*certificate)) {
      certificate_ = *certificate;
    } else {
      logger_->log_warn("Cannot open certificate file {}", *certificate);
      if (is_valid_and_readable_path(default_dir / *certificate)) {
        certificate_ = default_dir / *certificate;
      } else {
        logger_->log_error("Cannot open certificate file {}", (default_dir / *certificate));
      }
    }
  } else {
    logger_->log_debug("Certificate empty");
  }

  private_key_.clear();
  if (!certificate_.empty() && !isFileTypeP12(certificate_)) {
    if (auto private_key = getProperty(PrivateKey)) {
      if (is_valid_and_readable_path(*private_key)) {
        private_key_ = *private_key;
      } else {
        logger_->log_warn("Cannot open private key file {}", *private_key);
        if (is_valid_and_readable_path(default_dir / *private_key)) {
          private_key_ = default_dir / *private_key;
        } else {
          logger_->log_error("Cannot open private key file {}", (default_dir / *private_key));
        }
      }
      logger_->log_info("Using private key file {}", private_key_);
    } else {
      logger_->log_debug("Private key empty");
    }
  }

  passphrase_.clear();
  if (!getProperty(Passphrase, passphrase_)) {
    logger_->log_debug("No pass phrase for {}", certificate_);
  } else {
    std::ifstream passphrase_file(passphrase_);
    if (passphrase_file.good()) {
      // we should read it from the file
      passphrase_.assign((std::istreambuf_iterator<char>(passphrase_file)), std::istreambuf_iterator<char>());
    } else {
      auto test_passphrase = default_dir / passphrase_;
      std::ifstream passphrase_file_test(test_passphrase);
      if (passphrase_file_test.good()) {
        passphrase_.assign((std::istreambuf_iterator<char>(passphrase_file_test)), std::istreambuf_iterator<char>());
      } else {
        // not an invalid file since we support a passphrase of unencrypted text
      }
      passphrase_file_test.close();
    }
    passphrase_file.close();
  }

  ca_certificate_.clear();
  if (auto ca_certificate = getProperty(CACertificate)) {
    if (is_valid_and_readable_path(*ca_certificate)) {
      ca_certificate_ = *ca_certificate;
    } else {
      logger_->log_warn("Cannot open CA certificate file {}", *ca_certificate);
      if (is_valid_and_readable_path(default_dir / *ca_certificate)) {
        ca_certificate_ = default_dir / *ca_certificate;
      } else {
        logger_->log_error("Cannot open CA certificate file {}", (default_dir / *ca_certificate));
      }
    }
    logger_->log_info("Using CA certificate file {}", ca_certificate_);
  } else {
    logger_->log_debug("CA Certificate empty");
  }

  getProperty(UseSystemCertStore, use_system_cert_store_);

#ifdef WIN32
  getProperty(CertStoreLocation, cert_store_location_);
  getProperty(ServerCertStore, server_cert_store_);
  getProperty(ClientCertStore, client_cert_store_);
  getProperty(ClientCertCN, client_cert_cn_);

  std::string client_cert_key_usage;
  getProperty(ClientCertKeyUsage, client_cert_key_usage);
  client_cert_key_usage_ = utils::tls::ExtendedKeyUsage{client_cert_key_usage};
#endif  // WIN32

  logger_->log_debug("Using certificate file \"{}\"", certificate_);
  logger_->log_debug("Using private key file \"{}\"", private_key_);
  logger_->log_debug("Using CA certificate file \"{}\"", ca_certificate_);
  logger_->log_debug("Using the system cert store: {}", use_system_cert_store_ ? "yes" : "no");

  verifyCertificateExpiration();
}

void SSLContextServiceImpl::initializeProperties() {
  setSupportedProperties(Properties);
}

void SSLContextServiceImpl::verifyCertificateExpiration() {
  auto verify = [&] (const std::filesystem::path& cert_file, const utils::tls::X509_unique_ptr& cert) {
    if (auto end_date = utils::tls::getCertificateExpiration(cert)) {
      std::string end_date_str = utils::timeutils::getTimeStr(*end_date);
      if (end_date.value() < std::chrono::system_clock::now()) {
        logger_->log_error("Certificate in '{}' expired at {}", cert_file, end_date_str);
      } else if (auto diff = end_date.value() - std::chrono::system_clock::now(); diff < std::chrono::weeks{2}) {
        logger_->log_warn("Certificate in '{}' will expire at {}", cert_file, end_date_str);
      } else {
        logger_->log_debug("Certificate in '{}' will expire at {}", cert_file, end_date_str);
      }
    } else {
      logger_->log_error("Could not determine expiration date for certificate in '{}'", cert_file);
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
        logger_->log_error("{}", error.value());
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
        logger_->log_error("{}", error.value());
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
      logger_->log_error("{}", error.message());
    }
  }

#ifdef WIN32
  if (use_system_cert_store_ && IsNullOrEmpty(certificate_)) {
    findClientCertificate([&] (auto cert, auto /*priv_key*/) -> bool {
      auto cert_name = getCertName(cert);
      verify(cert_name, cert);
      return false;  // keep on iterating, check all
    });
  }

  if (use_system_cert_store_ && IsNullOrEmpty(ca_certificate_)) {
    findServerCertificate([&] (auto cert) -> bool {
      auto cert_name = getCertName(cert);
      verify(cert_name, cert);
      return false;  // keep on iterating, check all
    });
  }
#endif
}

REGISTER_RESOURCE_IMPLEMENTATION(SSLContextServiceImpl, "SSLContextService", ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
