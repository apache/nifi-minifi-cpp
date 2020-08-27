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
#include "utils/StringUtils.h"
#include "../TestBase.h"

namespace utils = org::apache::nifi::minifi::utils;

namespace {
const utils::crypto::Bytes SECRET_KEY = utils::crypto::stringToBytes(utils::StringUtils::from_base64(
    "qkEfKJyRaF751anlpPrZOT/0x6eKuXhIQyNIjK7Xqas="));

// also known as initialization vector or IV, it should be unique for each encryption, but does not need to be kept secret
const utils::crypto::Bytes NONCE = utils::crypto::stringToBytes(utils::StringUtils::from_base64(
    "RBrWo9lv7xNA6JJWHCa9avnT42CCr1bn"));
}  // namespace

TEST_CASE("EncryptionUtils can do a simple encryption", "[encrypt]") {
  std::string plaintext = "the attack begins at two";
  std::string aad = "additional data";  // not encrypted but included in the tag for authentication

  std::string output = utils::crypto::aeadEncrypt(plaintext, aad, SECRET_KEY, NONCE);

  REQUIRE(output.size() == plaintext.size() + EVP_AEAD_max_tag_len(EVP_aead_xchacha20_poly1305()));
  REQUIRE(utils::StringUtils::to_base64(output) ==
      "7dhhGeKQ1/XYYtklDUyWm52+pe7cH8tw"  // ciphertext
      "il21ODbMK6pAxDtCA5ABiw==");  // tag
}

TEST_CASE("EncryptionUtils can do a simple decryption", "[decrypt]") {
  std::string ciphertext = "7dhhGeKQ1/XYYtklDUyWm52+pe7cH8tw";
  std::string tag = "il21ODbMK6pAxDtCA5ABiw==";
  std::string aad = "additional data";  // must be the same string that was used during the encryption

  std::string output = utils::crypto::aeadDecrypt(utils::StringUtils::from_base64(ciphertext + tag), aad, SECRET_KEY, NONCE);

  REQUIRE(output == "the attack begins at two");
}

TEST_CASE("EncryptionUtils can generate random bytes", "[randomBytes]") {
  utils::crypto::Bytes key = utils::crypto::randomBytes(32);
  REQUIRE(key.size() == 32);

  // the following assertions will fail about once in every hundred and fifteen quattuorvigintillion runs,
  // which is much less likely than a test failure caused by a meteor strike destroying your computer
  auto is_zero = [](unsigned char byte) { return byte == 0; };
  REQUIRE_FALSE(std::all_of(key.begin(), key.end(), is_zero));

  utils::crypto::Bytes another_key = utils::crypto::randomBytes(32);
  REQUIRE(key != another_key);
}

TEST_CASE("EncryptionUtils can encrypt and decrypt strings using the simplified interface", "[encrypt][decrypt]") {
  utils::crypto::Bytes key = utils::crypto::randomBytes(32);
  std::string plaintext = "my social security number is 914-52-5373";
  std::string aad = "Rhode Island 1985";

  std::string encrypted_text = utils::crypto::encrypt(plaintext, aad, key);
  REQUIRE(encrypted_text.size() == 24 + plaintext.size() + 16);

  std::string decrypted_text = utils::crypto::decrypt(encrypted_text, aad, key);
  REQUIRE(decrypted_text == plaintext);
}
