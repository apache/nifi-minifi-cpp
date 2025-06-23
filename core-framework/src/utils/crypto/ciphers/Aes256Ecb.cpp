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
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::utils::crypto {

using EVP_CIPHER_CTX_ptr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

std::shared_ptr<core::logging::Logger> Aes256EcbCipher::logger_{core::logging::LoggerFactory<Aes256EcbCipher>::getLogger()};

Aes256EcbCipher::Aes256EcbCipher(Bytes encryption_key) : encryption_key_(std::move(encryption_key)) {
  if (encryption_key_.size() != KEY_SIZE) {
    handleError(fmt::format("Invalid key length {} bytes, expected {} bytes", encryption_key_.size(), static_cast<size_t>(KEY_SIZE)));
  }
}

void Aes256EcbCipher::handleOpenSSLError(const char* msg) {
  std::array<char, 128> errmsg = {0};
  const auto errcode = ERR_peek_last_error();
  if (!errcode) {
    handleError(fmt::format("{}: {}", msg, "Unknown OpenSSL error"));
  }
  ERR_error_string_n(errcode, errmsg.data(), errmsg.size());
  handleError(fmt::format("{}: {}", msg, errmsg.data()));
}

Bytes Aes256EcbCipher::generateKey() {
  return utils::crypto::randomBytes(KEY_SIZE);
}

void Aes256EcbCipher::encrypt(std::span<unsigned char, BLOCK_SIZE> data) const {
  gsl_Expects(data.size() == BLOCK_SIZE);
  EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx) {
    handleOpenSSLError("Could not create cipher context");
  }

  if (1 != EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_ecb(), nullptr, reinterpret_cast<const unsigned char*>(encryption_key_.data()), nullptr)) {
    handleOpenSSLError("Could not initialize encryption cipher context");
  }

  // EVP_EncryptFinal_ex pads the data even if there is none thus data that
  // is exactly BLOCK_SIZE long would result in 2*BLOCK_SIZE ciphertext
  if (1 != EVP_CIPHER_CTX_set_padding(ctx.get(), 0)) {
    handleOpenSSLError("Could not disable padding for cipher");
  }

  int ciphertext_len = 0;
  int len = 0;

  if (1 != EVP_EncryptUpdate(ctx.get(), data.data(), &len, data.data(), gsl::narrow<int>(data.size()))) {
    handleOpenSSLError("Could not update cipher content");
  }
  ciphertext_len += len;

  if (1 != EVP_EncryptFinal_ex(ctx.get(), data.data() + len, &len)) {
    handleOpenSSLError("Could not finalize encryption");
  }
  ciphertext_len += len;

  gsl_Expects(ciphertext_len == BLOCK_SIZE);
}

void Aes256EcbCipher::decrypt(std::span<unsigned char, BLOCK_SIZE> data) const {
  gsl_Expects(data.size() == BLOCK_SIZE);
  EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx) {
    handleOpenSSLError("Could not create cipher context");
  }

  if (1 != EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_ecb(), nullptr, reinterpret_cast<const unsigned char*>(encryption_key_.data()), nullptr)) {
    handleOpenSSLError("Could not initialize decryption cipher context");
  }

  // as we did not use padding during encryption
  if (1 != EVP_CIPHER_CTX_set_padding(ctx.get(), 0)) {
    handleOpenSSLError("Could not disable padding for cipher");
  }

  int plaintext_len = 0;
  int len = 0;

  if (1 != EVP_DecryptUpdate(ctx.get(), data.data(), &len, data.data(), gsl::narrow<int>(data.size()))) {
    handleOpenSSLError("Could not update cipher content");
  }
  plaintext_len += len;

  if (1 != EVP_DecryptFinal_ex(ctx.get(), data.data() + len, &len)) {
    handleOpenSSLError("Could not finalize decryption");
  }
  plaintext_len += len;

  gsl_Expects(plaintext_len == BLOCK_SIZE);
}

bool Aes256EcbCipher::operator==(const Aes256EcbCipher &other) const {
  gsl_Expects(encryption_key_.size() == KEY_SIZE);
  if (encryption_key_.size() != other.encryption_key_.size()) return false;
  return CRYPTO_memcmp(encryption_key_.data(), other.encryption_key_.data(), KEY_SIZE) == 0;
}

}  // namespace org::apache::nifi::minifi::utils::crypto
