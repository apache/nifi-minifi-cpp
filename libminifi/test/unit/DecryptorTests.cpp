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

#include "properties/Decryptor.h"
#include "TestUtils.h"

namespace minifi = org::apache::nifi::minifi;
namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("Decryptor can decide whether a property is encrypted", "[isEncrypted]") {
  utils::crypto::Bytes encryption_key;
  minifi::Decryptor decryptor{encryption_key};

  REQUIRE(minifi::Decryptor::isEncrypted(utils::nullopt) == false);
  REQUIRE(minifi::Decryptor::isEncrypted(utils::optional<std::string>{""}) == false);
  REQUIRE(minifi::Decryptor::isEncrypted(utils::optional<std::string>{"plaintext"}) == false);
  REQUIRE(minifi::Decryptor::isEncrypted(utils::optional<std::string>{"AES256-GCM"}) == false);
  REQUIRE(minifi::Decryptor::isEncrypted(utils::optional<std::string>{utils::crypto::EncryptionType::name()}) == true);
}

TEST_CASE("Decryptor can decrypt a property", "[decrypt]") {
  utils::crypto::Bytes encryption_key = utils::crypto::stringToBytes(utils::StringUtils::from_base64(
      "QCSzJ/3Jh84+tD3R9pC5mH5AcuACDj7fQ0nOGtkaTjg="));
  minifi::Decryptor decryptor{encryption_key};

  std::string encrypted_value = utils::StringUtils::from_base64(
      "xPE6MAJeR4JDboayYSSCNmpkksqvP28ZeuEw8m5kpTk6prqcHCbhbsc94EPqGFLHpGXbiZhpQSr7F8jMYiIML2c=");
  std::string aad = "nifi.security.client.pass.phrase";
  REQUIRE(decryptor.decrypt(encrypted_value, aad) == "CorrectHorseBatteryStaple");
}

TEST_CASE("Decryptor will throw if the value or the aad is incorrect", "[decrypt]") {
  utils::crypto::Bytes encryption_key = utils::crypto::stringToBytes(utils::StringUtils::from_base64(
      "QCSzJ/3Jh84+tD3R9pC5mH5AcuACDj7fQ0nOGtkaTjg="));
  minifi::Decryptor decryptor{encryption_key};

  std::string correct_value = utils::StringUtils::from_base64(
      "xPE6MAJeR4JDboayYSSCNmpkksqvP28ZeuEw8m5kpTk6prqcHCbhbsc94EPqGFLHpGXbiZhpQSr7F8jMYiIML2c=");
  std::string correct_aad = "nifi.security.client.pass.phrase";

  REQUIRE_THROWS(decryptor.decrypt("incorrect value", "incorrect aad"));
  REQUIRE_THROWS(decryptor.decrypt("incorrect value", correct_aad));
  REQUIRE_THROWS(decryptor.decrypt(correct_value, "incorrect aad"));
}

TEST_CASE("Decryptor can decrypt a configuration file", "[decryptSensitiveProperties]") {
  minifi::Configure configuration;
  configuration.setHome("resources");
  configuration.loadConfigureFile("encrypted.minifi.properties");
  REQUIRE(configuration.getConfiguredKeys().size() > 0);

  utils::crypto::Bytes encryption_key = utils::crypto::stringToBytes(utils::StringUtils::from_base64(
      "ECYjbr+6fn+9jjAmJBVBVvc3cYEUxaOm6zmp9iPHSvQ="));
  minifi::Decryptor decryptor{encryption_key};
  decryptor.decryptSensitiveProperties(configuration);

  utils::optional<std::string> passphrase = configuration.get("nifi.security.client.pass.phrase");
  REQUIRE(passphrase);
  REQUIRE(*passphrase == "SpeakFriendAndEnter");

  utils::optional<std::string> password = configuration.get("nifi.rest.api.password");
  REQUIRE(password);
  REQUIRE(*password == "OpenSesame");
}
