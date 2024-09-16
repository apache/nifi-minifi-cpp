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

#include <openssl/ssl.h>
#include <openssl/pkcs12.h>

#ifdef WIN32
#include <windows.h>
#include <wincrypt.h>
#endif  // WIN32

#include <memory>
#include <filesystem>
#include <optional>
#include <string>
#include <functional>

#include "WindowsCertStoreLocation.h"

namespace org::apache::nifi::minifi::utils::tls {

class ssl_error_category : public std::error_category {
 public:
  [[nodiscard]]
  const char* name() const noexcept override {
    return "ssl_error";
  }

  [[nodiscard]]
  static const ssl_error_category& get();

  [[nodiscard]]
  std::string message(int value) const override;
};

std::error_code get_last_ssl_error_code();

struct EVP_PKEY_deleter {
  void operator()(EVP_PKEY* pkey) const { EVP_PKEY_free(pkey); }
};
using EVP_PKEY_unique_ptr = std::unique_ptr<EVP_PKEY, EVP_PKEY_deleter>;

struct X509_deleter {
  void operator()(X509* cert) const { X509_free(cert); }
};
using X509_unique_ptr = std::unique_ptr<X509, X509_deleter>;

struct BIO_deleter {
  void operator()(BIO* bio) const  { BIO_free(bio); }
};
using BIO_unique_ptr = std::unique_ptr<BIO, BIO_deleter>;

struct PKCS12_deleter {
  void operator()(PKCS12* cert) const  { PKCS12_free(cert); }
};
using PKCS12_unique_ptr = std::unique_ptr<PKCS12, PKCS12_deleter>;

#ifdef WIN32
class WindowsCertStore {
 public:
  WindowsCertStore(const WindowsCertStoreLocation& loc, const std::string& cert_store);

  std::error_code error() const;

  PCCERT_CONTEXT nextCert();

  ~WindowsCertStore();

 private:
  std::error_code error_;
  HCERTSTORE store_ptr_;
  PCCERT_CONTEXT cert_ctx_ptr_ = nullptr;
};

// Returns nullptr on errors
X509_unique_ptr convertWindowsCertificate(PCCERT_CONTEXT certificate);

// Returns nullptr on errors, or if the input is not an RSA key pair
EVP_PKEY_unique_ptr convertWindowsRsaKeyPair(std::span<BYTE> data);

// Returns nullptr if the certificate has no associated private key, or the private key could not be extracted
EVP_PKEY_unique_ptr extractPrivateKey(PCCERT_CONTEXT certificate);
#endif  // WIN32

std::string getLatestOpenSSLErrorString();

std::optional<std::chrono::system_clock::time_point> getCertificateExpiration(const X509_unique_ptr& cert);

struct CertHandler {
  std::function<std::error_code(X509_unique_ptr cert)> cert_cb;
  std::function<std::error_code(X509_unique_ptr cert)> chain_cert_cb;
  std::function<std::error_code(EVP_PKEY_unique_ptr priv_key)> priv_key_cb;
};

std::error_code processP12Certificate(const std::filesystem::path& cert_file, const std::string& passphrase, const CertHandler& handler);

std::error_code processPEMCertificate(const std::filesystem::path& cert_file, const std::optional<std::string>& passphrase, const CertHandler& handler);

}  // namespace org::apache::nifi::minifi::utils::tls
