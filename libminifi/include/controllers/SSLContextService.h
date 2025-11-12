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
#include "core/controller/ControllerService.h"
#endif

#ifdef WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "openssl/bio.h"
#include "openssl/err.h"
#include "openssl/pkcs12.h"
#include "openssl/ssl.h"

#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/controller/ControllerServiceBase.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerFactory.h"
#include "io/validation.h"
#include "minifi-cpp/controllers/SSLContextServiceInterface.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "utils/ConfigurationUtils.h"
#include "minifi-cpp/utils/Export.h"
#include "utils/StringUtils.h"
#include "utils/tls/CertificateUtils.h"
#include "utils/tls/ExtendedKeyUsage.h"

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
class SSLContextService : public core::controller::ControllerServiceBase, public SSLContextServiceInterface {
 public:
  using ControllerServiceBase::ControllerServiceBase;

  static std::shared_ptr<SSLContextService> createAndEnable(std::string_view name, const std::shared_ptr<Configure> &configuration);

  static constexpr auto ImplementsApis = std::array{ SSLContextServiceInterface::ProvidesApi };

  void initialize() override;

  std::unique_ptr<SSLContext> createSSLContext();

  const std::filesystem::path& getCertificateFile() const override;
  const std::string& getPassphrase() const override;
  const std::filesystem::path& getPrivateKeyFile() const override;
  const std::filesystem::path& getCACertificate() const override;

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
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
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

 protected:
  virtual void initializeProperties();

  mutable std::mutex initialization_mutex_;
  bool initialized_{false};
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
    const auto err = ERR_peek_last_error();
    if (err == 0) {
      return "";
    }
    static_assert(utils::configuration::DEFAULT_BUFFER_SIZE >= 1);
    std::array<char, utils::configuration::DEFAULT_BUFFER_SIZE> buffer{};
    ERR_error_string_n(err, buffer.data(), buffer.size() - 1);
    return {buffer.data()};
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
};  // NOLINT the linter gets confused by the '{'s inside #ifdef's

}  // namespace org::apache::nifi::minifi::controllers
