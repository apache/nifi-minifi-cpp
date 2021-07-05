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

#include "utils/crypto/ciphers/Aes256Ecb.h"
#include "openssl/conf.h"
#include "openssl/evp.h"
#include "openssl/err.h"
#include "openssl/rand.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace crypto {

using EVP_CIPHER_CTX_ptr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

std::shared_ptr<core::logging::Logger> Aes256EcbCipher::logger_{core::logging::LoggerFactory<Aes256EcbCipher>::getLogger()};

Aes256EcbCipher::Aes256EcbCipher(Bytes encryption_key) : encryption_key_(std::move(encryption_key)) {
  if (encryption_key_.size() != KEY_SIZE) {
    handleError("Invalid key length %zu bytes, expected %zu bytes", encryption_key_.size(), static_cast<size_t>(KEY_SIZE));
  }
}

Bytes Aes256EcbCipher::generateKey() {
  unsigned char key[KEY_SIZE];
  if (1 != RAND_bytes(key, KEY_SIZE)) {
    handleError("Couldn't generate key");
  }
  return Bytes(key, key + KEY_SIZE);
}

void Aes256EcbCipher::encrypt(unsigned char *data) const {
  EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx) {
    handleError("Could not create cipher context");
  }

  if (1 != EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_ecb(), nullptr, encryption_key_.data(), nullptr)) {
    handleError("Could not initialize encryption cipher context");
  }

  if (1 != EVP_CIPHER_CTX_set_padding(ctx.get(), 0)) {
    handleError("Could not disable padding for cipher");
  }

  int ciphertext_len = 0;
  int len;

  if(1 != EVP_EncryptUpdate(ctx.get(), data, &len, data, BLOCK_SIZE)) {
    handleError("Could not update cipher content");
  }
  ciphertext_len += len;

  if(1 != EVP_EncryptFinal_ex(ctx.get(), data + len, &len)) {
    handleError("Could not finalize encryption");
  }
  ciphertext_len += len;

  gsl_Expects(ciphertext_len == BLOCK_SIZE);
}

void Aes256EcbCipher::decrypt(unsigned char *data) const {
  EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx) {
    handleError("Could not create cipher context");
  }

  if (1 != EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_ecb(), nullptr, encryption_key_.data(), nullptr)) {
    handleError("Could not initialize decryption cipher context");
  }

  if (1 != EVP_CIPHER_CTX_set_padding(ctx.get(), 0)) {
    handleError("Could not disable padding for cipher");
  }

  int plaintext_len = 0;
  int len;

  if(1 != EVP_DecryptUpdate(ctx.get(), data, &len, data, BLOCK_SIZE)) {
    handleError("Could not update cipher content");
  }
  plaintext_len += len;

  if(1 != EVP_DecryptFinal_ex(ctx.get(), data + len, &len)) {
    handleError("Could not finalize decryption");
  }
  plaintext_len += len;

  gsl_Expects(plaintext_len == BLOCK_SIZE);
}

bool Aes256EcbCipher::equals(const Aes256EcbCipher &other) const {
  if (encryption_key_.size() != other.encryption_key_.size()) return false;
  if (encryption_key_.size() != KEY_SIZE) return false;
  return CRYPTO_memcmp(encryption_key_.data(), other.encryption_key_.data(), KEY_SIZE) == 0;
}

}  // namespace crypto
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
