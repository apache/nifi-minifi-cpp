// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/crypto/EncryptionUtils.h"
#include "utils/StringUtils.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

namespace utils = org::apache::nifi::minifi::utils;

namespace {
const utils::crypto::Bytes SECRET_KEY = utils::string::from_hex("aa411f289c91685ef9d5a9e5a4fad9393ff4c7a78ab978484323488caed7a9ab");

const utils::crypto::Bytes NONCE = utils::string::from_base64("RBrWo9lv7xNA6JJWHCa9avnT42CCr1bn");
}  // namespace

TEST_CASE("EncryptionUtils can do a simple encryption", "[encryptRaw]") {
  utils::crypto::Bytes plaintext = utils::crypto::stringToBytes("the attack begins at two");

  utils::crypto::Bytes output = utils::crypto::encryptRaw(plaintext, SECRET_KEY, NONCE);

  REQUIRE(output.size() == plaintext.size() + utils::crypto::EncryptionType::macLength());
  REQUIRE(utils::string::to_base64(output) == "x3WIHJGb+7hGlfIQd3gz8zw11EP0uFh9Ml1XBEAPCX5OTKqWcY+o+Q==");
}

TEST_CASE("EncryptionUtils can do a simple decryption", "[decryptRaw]") {
  utils::crypto::Bytes ciphertext_plus_mac = utils::string::from_base64("x3WIHJGb+7hGlfIQd3gz8zw11EP0uFh9Ml1XBEAPCX5OTKqWcY+o+Q==");

  utils::crypto::Bytes output = utils::crypto::decryptRaw(ciphertext_plus_mac, SECRET_KEY, NONCE);

  REQUIRE(utils::crypto::bytesToString(output) == "the attack begins at two");
}

TEST_CASE("EncryptionUtils can generate random bytes", "[randomBytes][generateKey]") {
  std::function<utils::crypto::Bytes()> randomFunction;

  SECTION("generateKey() can generate random bytes") {
    randomFunction = utils::crypto::generateKey;
  }
  SECTION("randomBytes() can generate random bytes") {
    randomFunction = [](){ return utils::crypto::randomBytes(32); };
  }

  utils::crypto::Bytes random_bytes = randomFunction();
  REQUIRE(random_bytes.size() == 32);

  // the following assertions will fail about once in every hundred and fifteen quattuorvigintillion runs,
  // which is much less likely than a test failure caused by a meteor strike destroying your computer
  auto is_zero = [](std::byte b) { return static_cast<uint8_t>(b) == 0; };
  REQUIRE_FALSE(std::all_of(random_bytes.begin(), random_bytes.end(), is_zero));

  utils::crypto::Bytes different_random_bytes = randomFunction();
  REQUIRE(random_bytes != different_random_bytes);
}

TEST_CASE("EncryptionUtils can encrypt and decrypt strings using the simplified interface", "[encrypt][decrypt]") {
  utils::crypto::Bytes key = utils::crypto::generateKey();
  std::string plaintext = "my social security number is 914-52-5373";

  const auto base64_length = [](size_t raw_length) { return (raw_length + 2) / 3 * 4; };

  std::string encrypted_text = utils::crypto::encrypt(plaintext, key);
  REQUIRE(encrypted_text.size() ==
      base64_length(utils::crypto::EncryptionType::nonceLength()) +
      utils::crypto::EncryptionType::separator().size() +
      base64_length(plaintext.size() + utils::crypto::EncryptionType::macLength()));

  std::string decrypted_text = utils::crypto::decrypt(encrypted_text, key);
  REQUIRE(decrypted_text == plaintext);
}
