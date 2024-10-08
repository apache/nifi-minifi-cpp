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

#include "utils/crypto/EncryptionUtils.h"

#include <sodium.h>

#include <stdexcept>
#include <string>

#include "utils/StringUtils.h"
#include "utils/span.h"

namespace org::apache::nifi::minifi::utils::crypto {

Bytes stringToBytes(std::string_view text) {
  return utils::span_to<std::vector>(utils::as_span<const Bytes::value_type>(std::span(text)));
}

std::string bytesToString(const Bytes& bytes) {
  return utils::span_to<std::string>(utils::as_span<const char>(std::span(bytes)));
}

Bytes generateKey() {
  Bytes key(EncryptionType::keyLength());
  crypto_secretbox_keygen(reinterpret_cast<unsigned char*>(key.data()));
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
    throw EncryptionError{"Expected key of " + std::to_string(EncryptionType::keyLength()) +
        " bytes, but got " + std::to_string(key.size()) + " bytes during encryption"};
  }
  if (nonce.size() != EncryptionType::nonceLength()) {
    throw EncryptionError{"Expected nonce of " + std::to_string(EncryptionType::nonceLength()) +
        " bytes, but got " + std::to_string(nonce.size()) + " bytes during encryption"};
  }

  Bytes ciphertext_plus_mac(plaintext.size() + EncryptionType::macLength());
  crypto_secretbox_easy(reinterpret_cast<unsigned char*>(ciphertext_plus_mac.data()), reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size(),
      reinterpret_cast<const unsigned char*>(nonce.data()), reinterpret_cast<const unsigned char*>(key.data()));
    return ciphertext_plus_mac;
}

std::string encrypt(std::string_view plaintext, const Bytes& key) {
  Bytes nonce = randomBytes(EncryptionType::nonceLength());
  Bytes ciphertext_plus_mac = encryptRaw(stringToBytes(plaintext), key, nonce);

  std::string nonce_base64 = utils::string::to_base64(nonce);
  std::string ciphertext_plus_mac_base64 = utils::string::to_base64(ciphertext_plus_mac);
  return nonce_base64 + EncryptionType::separator() + ciphertext_plus_mac_base64;
}

Bytes decryptRaw(const Bytes& input, const Bytes& key, const Bytes& nonce) {
  if (key.size() != EncryptionType::keyLength()) {
    throw EncryptionError{"Expected key of " + std::to_string(EncryptionType::keyLength()) +
        " bytes, but got " + std::to_string(key.size()) + " bytes during decryption"};
  }
  if (nonce.size() != EncryptionType::nonceLength()) {
    throw EncryptionError{"Expected a nonce of " + std::to_string(EncryptionType::nonceLength()) +
        " bytes, but got " + std::to_string(nonce.size()) + " bytes during decryption"};
  }
  if (input.size() < EncryptionType::macLength()) {
    throw EncryptionError{"Input is too short: expected at least " + std::to_string(EncryptionType::macLength()) +
        " bytes, but got " + std::to_string(input.size()) + " bytes during decryption"};
  }

  Bytes plaintext(input.size() - EncryptionType::macLength());
  if (crypto_secretbox_open_easy(reinterpret_cast<unsigned char*>(plaintext.data()), reinterpret_cast<const unsigned char*>(input.data()), input.size(),
      reinterpret_cast<const unsigned char*>(nonce.data()), reinterpret_cast<const unsigned char*>(key.data()))) {
    throw EncryptionError{"Decryption failed; the input may be forged!"};
  }
  return plaintext;
}

std::string decrypt(std::string_view input, const Bytes& key) {
  auto data = parseEncrypted(input);
  Bytes plaintext = decryptRaw(data.ciphertext_plus_mac, key, data.nonce);
  return bytesToString(plaintext);
}

EncryptedData parseEncrypted(std::string_view input) {
  std::vector<std::string> nonce_and_rest = utils::string::split(input, EncryptionType::separator());
  if (nonce_and_rest.size() != 2) {
    throw EncryptionError{"Incorrect input; expected '<nonce>" + EncryptionType::separator() + "<ciphertext_plus_mac>'"};
  }

  Bytes nonce = utils::string::from_base64(nonce_and_rest[0]);
  Bytes ciphertext_plus_mac = utils::string::from_base64(nonce_and_rest[1]);

  return EncryptedData{nonce, ciphertext_plus_mac};
}

bool isEncrypted(std::string_view input) {
  try {
    parseEncrypted(input);
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace org::apache::nifi::minifi::utils::crypto
