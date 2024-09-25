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
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#ifdef WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/pkcs12.h>

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "utils/StringUtils.h"
#include "utils/tls/ExtendedKeyUsage.h"
#include "io/validation.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "utils/Export.h"
#include "utils/tls/CertificateUtils.h"
#include "minifi-cpp/controllers/SSLContextService.h"

namespace org::apache::nifi::minifi::controllers {

class SSLContext {
 public:
  SSLContext(SSL_CTX *context) // NOLINT
      : context_(context) {
  }
  ~SSLContext() {
    if (context_) {
      SSL_CTX_free(context_);
    }
  }
 protected:
  SSL_CTX *context_;
};

/**
 * SSLContextService provides a configurable controller service from
 * which we can provide an SSL Context or component parts that go
 * into creating one.
 *
 * Justification: Abstracts SSL support out of processors into a
 * configurable controller service.
 */
class SSLContextServiceImpl : public core::controller::ControllerServiceImpl, public SSLContextService {
 public:
  explicit SSLContextServiceImpl(std::string_view name, const utils::Identifier &uuid = {})
      : ControllerServiceImpl(name, uuid),
        initialized_(false),
        logger_(core::logging::LoggerFactory<SSLContextService>::getLogger(uuid_)) {
  }

  explicit SSLContextServiceImpl(std::string_view name, const std::shared_ptr<Configure> &configuration)
      : ControllerServiceImpl(name),
        initialized_(false),
        logger_(core::logging::LoggerFactory<SSLContextService>::getLogger(uuid_)) {
    setConfiguration(configuration);
    initialize();

    // set the properties based on the configuration
    std::string value;
    if (configuration_->get(Configure::nifi_security_client_certificate, value)) {
      setProperty(ClientCertificate, value);
    }

    if (configuration_->get(Configure::nifi_security_client_private_key, value)) {
      setProperty(PrivateKey, value);
    }

    if (configuration_->get(Configure::nifi_security_client_pass_phrase, value)) {
      setProperty(Passphrase, value);
    }

    if (configuration_->get(Configure::nifi_security_client_ca_certificate, value)) {
      setProperty(CACertificate, value);
    }

    if (configuration_->get(Configure::nifi_security_use_system_cert_store, value)) {
      setProperty(UseSystemCertStore, value);
    }

#ifdef WIN32
    if (configuration_->get(Configure::nifi_security_windows_cert_store_location, value)) {
      setProperty(CertStoreLocation, value);
    }

    if (configuration_->get(Configure::nifi_security_windows_server_cert_store, value)) {
      setProperty(ServerCertStore, value);
    }

    if (configuration_->get(Configure::nifi_security_windows_client_cert_store, value)) {
      setProperty(ClientCertStore, value);
    }

    if (configuration_->get(Configure::nifi_security_windows_client_cert_cn, value)) {
      setProperty(ClientCertCN, value);
    }

    if (configuration_->get(Configure::nifi_security_windows_client_cert_key_usage, value)) {
      setProperty(ClientCertKeyUsage, value);
    }
#endif  // WIN32
  }

  void initialize() override;

  std::unique_ptr<SSLContext> createSSLContext();

  const std::filesystem::path& getCertificateFile() const override;
  const std::string& getPassphrase() const override;
  const std::filesystem::path& getPrivateKeyFile() const override;
  const std::filesystem::path& getCACertificate() const override;

  void yield() override {
  }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() override {
    return false;
  }

  void setMinTlsVersion(long min_version) override {  // NOLINT(runtime/int) long due to SSL lib API
    minimum_tls_version_ = min_version;
  }

  void setMaxTlsVersion(long max_version) override {  // NOLINT(runtime/int) long due to SSL lib API
    maximum_tls_version_ = max_version;
  }
  bool configure_ssl_context(void* ctx) override;

  void onEnable() override;

  MINIFIAPI static constexpr const char* Description = "Controller service that provides SSL/TLS capabilities to consuming interfaces";

#ifdef WIN32
  MINIFIAPI static constexpr auto CertStoreLocation = core::PropertyDefinitionBuilder<utils::tls::WindowsCertStoreLocation::SIZE>::createProperty("Certificate Store Location")
      .withDescription("One of the Windows certificate store locations, eg. LocalMachine or CurrentUser (Windows only)")
      .withAllowedValues(utils::tls::WindowsCertStoreLocation::LOCATION_NAMES)
      .isRequired(false)
      .withDefaultValue(utils::tls::WindowsCertStoreLocation::DEFAULT_LOCATION)
      .build();
  MINIFIAPI static constexpr auto ServerCertStore = core::PropertyDefinitionBuilder<>::createProperty("Server Cert Store")
      .withDescription("The name of the certificate store which contains the server certificate (Windows only)")
      .isRequired(false)
      .withDefaultValue("ROOT")
      .build();
  MINIFIAPI static constexpr auto ClientCertStore = core::PropertyDefinitionBuilder<>::createProperty("Client Cert Store")
      .withDescription("The name of the certificate store which contains the client certificate (Windows only)")
      .isRequired(false)
      .withDefaultValue("MY")
      .build();
  MINIFIAPI static constexpr auto ClientCertCN = core::PropertyDefinitionBuilder<>::createProperty("Client Cert CN")
      .withDescription("The CN that the client certificate is required to match; default: use the first available client certificate in the store (Windows only)")
      .isRequired(false)
      .build();
  MINIFIAPI static constexpr auto ClientCertKeyUsage = core::PropertyDefinitionBuilder<>::createProperty("Client Cert Key Usage")
      .withDescription("Comma-separated list of enhanced key usage values that the client certificate is required to have (Windows only)")
      .isRequired(false)
      .withDefaultValue("Client Authentication")
      .build();
#endif  // WIN32
  MINIFIAPI static constexpr auto ClientCertificate = core::PropertyDefinitionBuilder<>::createProperty("Client Certificate")
      .withDescription("Client Certificate")
      .isRequired(false)
      .build();
  MINIFIAPI static constexpr auto PrivateKey = core::PropertyDefinitionBuilder<>::createProperty("Private Key")
      .withDescription("Private Key file")
      .isRequired(false)
      .build();
  MINIFIAPI static constexpr auto Passphrase = core::PropertyDefinitionBuilder<>::createProperty("Passphrase")
      .withDescription("Client passphrase. Either a file or unencrypted text")
      .isRequired(false)
      .isSensitive(true)
      .build();
  MINIFIAPI static constexpr auto CACertificate = core::PropertyDefinitionBuilder<>::createProperty("CA Certificate")
      .withDescription("CA certificate file")
      .isRequired(false)
      .build();
  MINIFIAPI static constexpr auto UseSystemCertStore = core::PropertyDefinitionBuilder<>::createProperty("Use System Cert Store")
      .withDescription("Whether to use the certificates in the OS's certificate store")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  MINIFIAPI static constexpr auto Properties =
#ifdef WIN32
      std::to_array<core::PropertyReference>({
          CertStoreLocation,
          ServerCertStore,
          ClientCertStore,
          ClientCertCN,
          ClientCertKeyUsage,
#else
      std::to_array<core::PropertyReference>({
#endif  // WIN32
                                                 ClientCertificate,
                                                 PrivateKey,
                                                 Passphrase,
                                                 CACertificate,
                                                 UseSystemCertStore
                                             });

  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

 protected:
  virtual void initializeProperties();

  mutable std::mutex initialization_mutex_;
  bool initialized_;
  std::filesystem::path certificate_;
  std::filesystem::path private_key_;
  std::string passphrase_;
  std::filesystem::path ca_certificate_;
  bool use_system_cert_store_ = false;
#ifdef WIN32
  std::string cert_store_location_;
  std::string server_cert_store_;
  std::string client_cert_store_;
  std::string client_cert_cn_;
  utils::tls::ExtendedKeyUsage client_cert_key_usage_;
#endif  // WIN32

  static std::string getLatestOpenSSLErrorString() {
    unsigned long err = ERR_peek_last_error(); // NOLINT
    if (err == 0U) {
      return "";
    }
    char buf[4096];
    ERR_error_string_n(err, buf, sizeof(buf));
    return buf;
  }

  static bool isFileTypeP12(const std::filesystem::path& filename) {
    return utils::string::endsWith(filename.string(), "p12", false);
  }

 private:
  bool addP12CertificateToSSLContext(SSL_CTX* ctx) const;
  bool addPemCertificateToSSLContext(SSL_CTX* ctx) const;
  bool addClientCertificateFromSystemStoreToSSLContext(SSL_CTX* ctx) const;
  bool addServerCertificatesFromSystemStoreToSSLContext(SSL_CTX* ctx) const;
#ifdef WIN32
  using ClientCertCallback = std::function<bool(utils::tls::X509_unique_ptr cert, utils::tls::EVP_PKEY_unique_ptr priv_key)>;
  using ServerCertCallback = std::function<bool(utils::tls::X509_unique_ptr cert)>;

  bool findClientCertificate(ClientCertCallback cb) const;
  bool findServerCertificate(ServerCertCallback cb) const;

  bool useClientCertificate(PCCERT_CONTEXT certificate, ClientCertCallback cb) const;
  bool useServerCertificate(PCCERT_CONTEXT certificate, ServerCertCallback cb) const;
#endif  // WIN32
  long minimum_tls_version_ = -1;  // NOLINT(runtime/int) long due to SSL lib API
  long maximum_tls_version_ = -1;  // NOLINT(runtime/int) long due to SSL lib API

  void verifyCertificateExpiration();

  std::shared_ptr<core::logging::Logger> logger_;
};  // NOLINT the linter gets confused by the '{'s inside #ifdef's

}  // namespace org::apache::nifi::minifi::controllers
