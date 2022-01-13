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
#ifdef OPENSSL_SUPPORT

#include "utils/tls/CertificateUtils.h"

#include <openssl/rsa.h>
#include <openssl/err.h>

#ifdef WIN32
#include <winsock2.h>

#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "Ws2_32.lib")
#endif  // WIN32

#include "utils/StringUtils.h"
#include "utils/tls/TLSUtils.h"
#include "utils/TimeUtil.h"

namespace org::apache::nifi::minifi::utils::tls {

const ssl_error_category& ssl_error_category::get() {
  static ssl_error_category instance;
  return instance;
}

std::string ssl_error_category::message(int value) const {
  auto err = gsl::narrow<unsigned long>(value);  // NOLINT
  if (err == 0) {
    return "";
  }
  char buf[4096];
  ERR_error_string_n(err, buf, sizeof(buf));
  return buf;
}

std::error_code get_last_ssl_error_code() {
  return std::error_code{gsl::narrow<int>(ERR_peek_last_error()), ssl_error_category::get()};
}

#ifdef WIN32
WindowsCertStore::WindowsCertStore(const WindowsCertStoreLocation& loc, const std::string& cert_store) {
  store_ptr_ = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, NULL,
                             CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG | loc.getBitfieldValue(),
                             cert_store.data());

  if (!store_ptr_) {
    error_ = std::error_code{WSAGetLastError(), std::system_category()};
  }
}

std::error_code WindowsCertStore::error() const {
  return error_;
}

PCCERT_CONTEXT WindowsCertStore::nextCert() {
  return cert_ctx_ptr_ = CertEnumCertificatesInStore(store_ptr_, cert_ctx_ptr_);
}

WindowsCertStore::~WindowsCertStore() {
  if (cert_ctx_ptr_) {
    CertFreeCertificateContext(cert_ctx_ptr_);
  }
  if (store_ptr_) {
    CertCloseStore(store_ptr_, 0);
  }
}

X509_unique_ptr convertWindowsCertificate(const PCCERT_CONTEXT certificate) {
  const unsigned char *certificate_binary = certificate->pbCertEncoded;
  long certificate_length = certificate->cbCertEncoded;  // NOLINT: cpplint hates `long`, but that is the param type in the API
  return X509_unique_ptr{d2i_X509(nullptr, &certificate_binary, certificate_length)};
}

// from Shane Powell's answer at https://stackoverflow.com/questions/60180688, used with permission
EVP_PKEY_unique_ptr extractPrivateKey(const PCCERT_CONTEXT certificate) {
  HCRYPTPROV_OR_NCRYPT_KEY_HANDLE key_handle;
  DWORD key_spec = 0;
  BOOL free_key;
  if (!CryptAcquireCertificatePrivateKey(certificate,
                                         CRYPT_ACQUIRE_ONLY_NCRYPT_KEY_FLAG | CRYPT_ACQUIRE_SILENT_FLAG,
                                         nullptr,
                                         &key_handle,
                                         &key_spec,
                                         &free_key)) {
    return nullptr;
  }

  EVP_PKEY_unique_ptr pkey = nullptr;
  DWORD length = 0;
  if (SUCCEEDED(NCryptExportKey(key_handle, NULL, BCRYPT_RSAFULLPRIVATE_BLOB, nullptr, nullptr, 0, &length, 0))) {
    auto data = std::make_unique<BYTE[]>(length);

    if (SUCCEEDED(NCryptExportKey(key_handle,
                                  NULL,
                                  BCRYPT_RSAFULLPRIVATE_BLOB,
                                  nullptr,
                                  data.get(),
                                  length,
                                  &length,
                                  0))) {
      // https://docs.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_rsakey_blob
      auto const blob = reinterpret_cast<BCRYPT_RSAKEY_BLOB *>(data.get());

      if (blob->Magic == BCRYPT_RSAFULLPRIVATE_MAGIC) {
        auto rsa = RSA_new();

        // n is the modulus common to both public and private key
        auto const n = BN_bin2bn(data.get() + sizeof(BCRYPT_RSAKEY_BLOB) + blob->cbPublicExp, blob->cbModulus, nullptr);
        // e is the public exponent
        auto const e = BN_bin2bn(data.get() + sizeof(BCRYPT_RSAKEY_BLOB), blob->cbPublicExp, nullptr);
        // d is the private exponent
        auto const d = BN_bin2bn(data.get() + sizeof(BCRYPT_RSAKEY_BLOB) + blob->cbPublicExp + blob->cbModulus + blob->cbPrime1
            + blob->cbPrime2 + blob->cbPrime1 + blob->cbPrime2 + blob->cbPrime1, blob->cbModulus, nullptr);

        RSA_set0_key(rsa, n, e, d);

        // p and q are the first and second factor of n
        auto const p = BN_bin2bn(data.get() + sizeof(BCRYPT_RSAKEY_BLOB) + blob->cbPublicExp + blob->cbModulus,
                                 blob->cbPrime1, nullptr);
        auto const q = BN_bin2bn(data.get() + sizeof(BCRYPT_RSAKEY_BLOB) + blob->cbPublicExp + blob->cbModulus + blob->cbPrime1,
                                 blob->cbPrime2, nullptr);

        RSA_set0_factors(rsa, p, q);

        // dmp1, dmq1 and iqmp are the exponents and coefficient for CRT calculations
        auto const dmp1 = BN_bin2bn(data.get() + sizeof(BCRYPT_RSAKEY_BLOB) + blob->cbPublicExp + blob->cbModulus + blob->cbPrime1
            + blob->cbPrime2, blob->cbPrime1, nullptr);
        auto const dmq1 = BN_bin2bn(data.get() + sizeof(BCRYPT_RSAKEY_BLOB) + blob->cbPublicExp + blob->cbModulus + blob->cbPrime1
            + blob->cbPrime2 + blob->cbPrime1, blob->cbPrime2, nullptr);
        auto const iqmp = BN_bin2bn(data.get() + sizeof(BCRYPT_RSAKEY_BLOB) + blob->cbPublicExp + blob->cbModulus + blob->cbPrime1
            + blob->cbPrime2 + blob->cbPrime1 + blob->cbPrime2, blob->cbPrime1, nullptr);

        RSA_set0_crt_params(rsa, dmp1, dmq1, iqmp);

        pkey.reset(EVP_PKEY_new());

        // ownership of rsa transferred to pkey
        EVP_PKEY_assign_RSA(pkey.get(), rsa);
      }
    }
  }

  if (free_key) {
    NCryptFreeObject(key_handle);
  }

  return pkey;
}
#endif  // WIN32

std::string getLatestOpenSSLErrorString() {
  return get_last_ssl_error_code().message();
}

std::optional<std::chrono::system_clock::time_point> getCertificateExpiration(const X509_unique_ptr& cert) {
  const ASN1_TIME* asn1_end = X509_get0_notAfter(cert.get());
  if (!asn1_end) {
    return {};
  }
  std::tm end{};
  int ret = ASN1_TIME_to_tm(asn1_end, &end);
  if (ret == -1) {
    return {};
  }
  return utils::timeutils::to_sys_time(end);
}

std::error_code processP12Certificate(const std::filesystem::path& cert_file, const std::string& passphrase, const CertHandler& handler) {
  utils::tls::BIO_unique_ptr fp{BIO_new(BIO_s_file())};
  if (fp == nullptr) {
    return get_last_ssl_error_code();
  }
  if (BIO_read_filename(fp.get(), cert_file.string().c_str()) <= 0) {
    return get_last_ssl_error_code();
  }
  utils::tls::PKCS12_unique_ptr  p12{d2i_PKCS12_bio(fp.get(), nullptr)};
  if (p12 == nullptr) {
    return get_last_ssl_error_code();
  }

  EVP_PKEY* pkey = nullptr;
  X509* cert = nullptr;
  STACK_OF(X509)* ca = nullptr;
  if (!PKCS12_parse(p12.get(), passphrase.c_str(), &pkey, &cert, &ca)) {
    return get_last_ssl_error_code();
  }
  utils::tls::EVP_PKEY_unique_ptr pkey_ptr{pkey};
  utils::tls::X509_unique_ptr cert_ptr{cert};
  const auto ca_deleter = gsl::finally([ca] { sk_X509_pop_free(ca, X509_free); });

  if (handler.cert_cb) {
    if (auto error = handler.cert_cb(std::move(cert_ptr))) {
      return error;
    }
  }

  if (handler.chain_cert_cb) {
    while (ca != nullptr && sk_X509_num(ca) > 0) {
      if (auto error = handler.chain_cert_cb(utils::tls::X509_unique_ptr{sk_X509_pop(ca)})) {
        return error;
      }
    }
  }

  if (handler.priv_key_cb) {
    return handler.priv_key_cb(std::move(pkey_ptr));
  }

  return {};
}

std::error_code processPEMCertificate(const std::filesystem::path& cert_file, const std::optional<std::string>& passphrase, const CertHandler& handler) {
  utils::tls::BIO_unique_ptr fp{BIO_new(BIO_s_file())};
  if (fp == nullptr) {
    return get_last_ssl_error_code();
  }
  if (BIO_read_filename(fp.get(), cert_file.string().c_str()) <= 0) {
    return get_last_ssl_error_code();
  }
  std::decay_t<decltype(pemPassWordCb)> pwd_cb = nullptr;
  void* pwd_data = nullptr;
  if (passphrase) {
    pwd_cb = pemPassWordCb;
    pwd_data = const_cast<std::string*>(&passphrase.value());
  }

  X509_unique_ptr cert{PEM_read_bio_X509_AUX(fp.get(), nullptr, pwd_cb, pwd_data)};
  if (!cert) {
    return get_last_ssl_error_code();
  }

  if (handler.cert_cb) {
    if (auto error = handler.cert_cb(std::move(cert))) {
      return error;
    }
  }

  if (handler.chain_cert_cb) {
    while (X509_unique_ptr chain_cert{PEM_read_bio_X509(fp.get(), nullptr, pwd_cb, pwd_data)}) {
      if (auto error = handler.chain_cert_cb(std::move(chain_cert))) {
        return error;
      }
    }
  }

  return {};
}

}  // namespace org::apache::nifi::minifi::utils::tls

#endif  // OPENSSL_SUPPORT
