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
#ifdef OPENSSL_SUPPORT

#include <openssl/ssl.h>

#ifdef WIN32
#include <windows.h>
#include <wincrypt.h>
#endif  // WIN32

#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace tls {

struct EVP_PKEY_deleter {
  void operator()(EVP_PKEY* pkey) const { EVP_PKEY_free(pkey); }
};
using EVP_PKEY_unique_ptr = std::unique_ptr<EVP_PKEY, EVP_PKEY_deleter>;

struct X509_deleter {
  void operator()(X509* cert) const { X509_free(cert); }
};
using X509_unique_ptr = std::unique_ptr<X509, X509_deleter>;

#ifdef WIN32
// Returns nullptr on errors
X509_unique_ptr convertWindowsCertificate(PCCERT_CONTEXT certificate);

// Returns nullptr if the certificate has no associated private key, or the private key could not be extracted
EVP_PKEY_unique_ptr extractPrivateKey(PCCERT_CONTEXT certificate);
#endif  // WIN32

}  // namespace tls
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // OPENSSL_SUPPORT
