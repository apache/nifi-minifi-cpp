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

#ifdef WIN32
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "Ws2_32.lib")
#endif  // WIN32

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace tls {

#ifdef WIN32
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

}  // namespace tls
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // OPENSSL_SUPPORT
