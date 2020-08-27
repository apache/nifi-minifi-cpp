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

#include "utils/EncryptionUtils.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <stdexcept>
#include <string>

#include "utils/gsl.h"

namespace {
std::string dataToString(const unsigned char* data, size_t len) {
  return std::string(reinterpret_cast<const char*>(data), len);
}
}  // namespace

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace crypto {

Bytes stringToBytes(const std::string& text) {
  return Bytes(text.begin(), text.end());
}

std::string bytesToString(const Bytes& bytes) {
  return dataToString(bytes.data(), bytes.size());
}

Bytes randomBytes(size_t num_bytes) {
  Bytes random_bytes(num_bytes);
  RAND_bytes(random_bytes.data(), gsl::narrow<int>(num_bytes));
  return random_bytes;
}

const EVP_AEAD* EncryptionType::cipher() { return EVP_aead_xchacha20_poly1305(); }

std::string EncryptionType::name() { return "XChaCha20-Poly1305"; }

size_t EncryptionType::nonceLength() { return EVP_AEAD_nonce_length(cipher()); }

std::string aeadEncrypt(const std::string& input, const std::string& aad,
                        const Bytes& key, const Bytes& nonce) {
  if (nonce.size() != EncryptionType::nonceLength()) {
    throw std::invalid_argument{"Expected a nonce of " + std::to_string(EncryptionType::nonceLength()) +
        " bytes, but got " + std::to_string(nonce.size()) + " bytes during encryption"};
  }

  EVP_AEAD_CTX context;
  const auto context_cleanup = gsl::finally([&context](){ EVP_AEAD_CTX_cleanup(&context); });

  const EVP_AEAD* cipher = EncryptionType::cipher();

  if (EVP_AEAD_CTX_init(&context, cipher, key.data(), key.size(),
                        EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr) != 1) {
    throw std::runtime_error{"Failed to initialize the AEAD cipher during encryption"};
  }

  Bytes input_bytes = stringToBytes(input);
  Bytes aad_bytes = stringToBytes(aad);

  size_t max_output_len = input.size() + EVP_AEAD_max_tag_len(cipher);
  Bytes output(max_output_len);
  size_t output_len;

  if (EVP_AEAD_CTX_seal(&context, output.data(), &output_len, max_output_len,
                        nonce.data(), nonce.size(),
                        input_bytes.data(), input_bytes.size(),
                        aad_bytes.data(), aad_bytes.size()) != 1) {
    throw std::runtime_error{"AEAD seal operation failed during encryption"};
  }

  return dataToString(output.data(), output_len);
}

std::string encrypt(const std::string& plaintext, const std::string& aad, const Bytes& key) {
  Bytes nonce = randomBytes(EncryptionType::nonceLength());
  std::string ciphertext_plus_tag = aeadEncrypt(plaintext, aad, key, nonce);
  return bytesToString(nonce) + ciphertext_plus_tag;
}

std::string aeadDecrypt(const std::string& input, const std::string& aad,
                        const Bytes& key, const Bytes& nonce) {
  if (nonce.size() != EncryptionType::nonceLength()) {
    throw std::invalid_argument{"Expected a nonce of " + std::to_string(EncryptionType::nonceLength()) +
        " bytes, but got " + std::to_string(nonce.size()) + " bytes during decryption"};
  }

  EVP_AEAD_CTX context;
  const auto context_cleanup = gsl::finally([&context](){ EVP_AEAD_CTX_cleanup(&context); });

  const EVP_AEAD* cipher = EncryptionType::cipher();

  if (EVP_AEAD_CTX_init(&context, cipher, key.data(), key.size(),
                        EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr) != 1) {
    throw std::runtime_error{"Failed to initialize the AEAD cipher during decryption"};
  }

  Bytes input_bytes = stringToBytes(input);
  Bytes aad_bytes = stringToBytes(aad);

  size_t max_output_len = input.size();
  Bytes output(max_output_len);
  size_t output_len;

  if (EVP_AEAD_CTX_open(&context, output.data(), &output_len, max_output_len,
                        nonce.data(), nonce.size(),
                        input_bytes.data(), input_bytes.size(),
                        aad_bytes.data(), aad_bytes.size()) != 1) {
    throw std::runtime_error{"AEAD open operation failed during decryption"};
  }

  return dataToString(output.data(), output_len);
}

std::string decrypt(const std::string& input, const std::string& aad, const Bytes& key) {
  Bytes nonce = stringToBytes(input.substr(0, EncryptionType::nonceLength()));
  std::string ciphertext_plus_tag = input.substr(EncryptionType::nonceLength());
  return aeadDecrypt(ciphertext_plus_tag, aad, key, nonce);
}

}  // namespace crypto
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
