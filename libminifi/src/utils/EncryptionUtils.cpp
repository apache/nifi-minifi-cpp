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

#include <sodium.h>

#include <stdexcept>
#include <string>

#include "utils/StringUtils.h"

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
  return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

Bytes generateKey() {
  Bytes key(EncryptionType::keyLength());
  crypto_secretbox_keygen(key.data());
  return key;
}

Bytes randomBytes(size_t num_bytes) {
  Bytes random_bytes(num_bytes);
  randombytes_buf(random_bytes.data(), num_bytes);
  return random_bytes;
}

std::string EncryptionType::name() { return crypto_secretbox_primitive(); }

size_t EncryptionType::keyLength() { return crypto_secretbox_keybytes(); }

size_t EncryptionType::nonceLength() { return crypto_secretbox_noncebytes(); }

size_t EncryptionType::macLength() { return crypto_secretbox_macbytes(); }

std::string EncryptionType::separator() { return "||"; }

Bytes encryptRaw(const Bytes& plaintext, const Bytes& key, const Bytes& nonce) {
  if (key.size() != EncryptionType::keyLength()) {
    throw std::invalid_argument{"Expected key of " + std::to_string(EncryptionType::keyLength()) +
        " bytes, but got " + std::to_string(key.size()) + " bytes during encryption"};
  }
  if (nonce.size() != EncryptionType::nonceLength()) {
    throw std::invalid_argument{"Expected nonce of " + std::to_string(EncryptionType::nonceLength()) +
        " bytes, but got " + std::to_string(nonce.size()) + " bytes during encryption"};
  }

  Bytes ciphertext_plus_mac(plaintext.size() + EncryptionType::macLength());
  crypto_secretbox_easy(ciphertext_plus_mac.data(), plaintext.data(), plaintext.size(), nonce.data(), key.data());
  return ciphertext_plus_mac;
}

std::string encrypt(const std::string& plaintext, const Bytes& key) {
  Bytes nonce = randomBytes(EncryptionType::nonceLength());
  Bytes ciphertext_plus_mac = encryptRaw(stringToBytes(plaintext), key, nonce);

  std::string nonce_base64 = utils::StringUtils::to_base64(nonce);
  std::string ciphertext_plus_mac_base64 = utils::StringUtils::to_base64(ciphertext_plus_mac);
  return nonce_base64 + EncryptionType::separator() + ciphertext_plus_mac_base64;
}

Bytes decryptRaw(const Bytes& input, const Bytes& key, const Bytes& nonce) {
  if (key.size() != EncryptionType::keyLength()) {
    throw std::invalid_argument{"Expected key of " + std::to_string(EncryptionType::keyLength()) +
        " bytes, but got " + std::to_string(key.size()) + " bytes during decryption"};
  }
  if (nonce.size() != EncryptionType::nonceLength()) {
    throw std::invalid_argument{"Expected a nonce of " + std::to_string(EncryptionType::nonceLength()) +
        " bytes, but got " + std::to_string(nonce.size()) + " bytes during decryption"};
  }
  if (input.size() < EncryptionType::macLength()) {
    throw std::invalid_argument{"Input is too short: expected at least " + std::to_string(EncryptionType::macLength()) +
        " bytes, but got " + std::to_string(input.size()) + " bytes during decryption"};
  }

  Bytes plaintext(input.size() - EncryptionType::macLength());
  if (crypto_secretbox_open_easy(plaintext.data(), input.data(), input.size(), nonce.data(), key.data())) {
    throw std::runtime_error{"Decryption failed; the input may be forged!"};
  }
  return plaintext;
}

std::string decrypt(const std::string& input, const Bytes& key) {
  std::vector<std::string> nonce_and_rest = utils::StringUtils::split(input, EncryptionType::separator());
  if (nonce_and_rest.size() != 2) {
    throw std::invalid_argument{"Incorrect input; expected '<nonce>" + EncryptionType::separator() + "<ciphertext_plus_mac>'"};
  }

  Bytes nonce = utils::StringUtils::from_base64(nonce_and_rest[0].data(), nonce_and_rest[0].size());
  Bytes ciphertext_plus_mac = utils::StringUtils::from_base64(nonce_and_rest[1].data(), nonce_and_rest[1].size());

  Bytes plaintext = decryptRaw(ciphertext_plus_mac, key, nonce);
  return bytesToString(plaintext);
}

}  // namespace crypto
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
