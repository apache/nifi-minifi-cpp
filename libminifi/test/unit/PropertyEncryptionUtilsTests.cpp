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

#include <string>

#include "../Catch.h"
#include "utils/crypto/EncryptionProvider.h"
#include "utils/crypto/EncryptionUtils.h"
#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"
#include "utils/StringUtils.h"

namespace utils = org::apache::nifi::minifi::utils;
namespace crypto = utils::crypto;
namespace property_encryption = crypto::property_encryption;

const crypto::Bytes secret_key = crypto::generateKey();
const crypto::EncryptionProvider property_encryptor{secret_key};

TEST_CASE("A property value can be encrypted") {
  std::string encrypted_blank = property_encryption::encrypt("", property_encryptor);
  CHECK(encrypted_blank.starts_with("enc{"));
  CHECK(encrypted_blank.ends_with("}"));
  CHECK(encrypted_blank.size() == 63);

  std::string encrypted_foo = property_encryption::encrypt("foo", property_encryptor);
  CHECK(encrypted_foo.starts_with("enc{"));
  CHECK(encrypted_foo.ends_with("}"));
  CHECK(encrypted_foo.size() == 67);

  std::string encrypted_long_text = property_encryption::encrypt("Lasciate ogne speranza, voi ch’intrate’. "
      "Queste parole di colore oscuro vid’ïo scritte al sommo d’una porta; per ch’io: \"Maestro, il senso lor m’è duro\".", property_encryptor);
  CHECK(encrypted_long_text.starts_with("enc{"));
  CHECK(encrypted_long_text.ends_with("}"));
  CHECK(encrypted_long_text.size() == 283);
}

TEST_CASE("Encrypting the same value a second time doesn't change its value") {
  std::string encrypted_foo = property_encryption::encrypt("foo", property_encryptor);
  std::string re_encrypted_foo = property_encryption::encrypt(encrypted_foo, property_encryptor);
  CHECK(encrypted_foo == re_encrypted_foo);

  std::string re_re_encrypted_foo = property_encryption::encrypt(re_encrypted_foo, property_encryptor);
  CHECK(encrypted_foo == re_re_encrypted_foo);
}

TEST_CASE("We can decrypt an encrypted value") {
  std::string encrypted_blank = property_encryption::encrypt("", property_encryptor);
  CHECK(property_encryption::decrypt(encrypted_blank, property_encryptor).empty());

  std::string encrypted_foo = property_encryption::encrypt("foo", property_encryptor);
  CHECK(property_encryption::decrypt(encrypted_foo, property_encryptor) == "foo");

  std::string random_value = utils::string::to_base64(crypto::generateKey());
  std::string encrypted_random_value = property_encryption::encrypt(random_value, property_encryptor);
  std::string decrypted_random_value = property_encryption::decrypt(encrypted_random_value, property_encryptor);
  CHECK(decrypted_random_value == random_value);
}

TEST_CASE("Decrypting a non-encrypted value doesn't do anything") {
  CHECK(property_encryption::decrypt("", property_encryptor).empty());
  CHECK(property_encryption::decrypt("foo", property_encryptor) == "foo");
  CHECK(property_encryption::decrypt("enc(foo)", property_encryptor) == "enc(foo)");
}
