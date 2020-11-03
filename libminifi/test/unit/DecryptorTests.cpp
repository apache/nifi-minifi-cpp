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

TEST_CASE("Decryptor can decide whether a property is encrypted", "[isValidEncryptionMarker]") {
  utils::crypto::Bytes encryption_key;
  minifi::Decryptor decryptor{utils::crypto::EncryptionProvider{encryption_key}};

  REQUIRE(minifi::Decryptor::isValidEncryptionMarker(utils::nullopt) == false);
  REQUIRE(minifi::Decryptor::isValidEncryptionMarker(utils::optional<std::string>{""}) == false);
  REQUIRE(minifi::Decryptor::isValidEncryptionMarker(utils::optional<std::string>{"plaintext"}) == false);
  REQUIRE(minifi::Decryptor::isValidEncryptionMarker(utils::optional<std::string>{"AES256-GCM"}) == false);
  REQUIRE(
      minifi::Decryptor::isValidEncryptionMarker(utils::optional<std::string>{utils::crypto::EncryptionType::name()}) == true);
}

TEST_CASE("Decryptor can decrypt a property", "[decrypt]") {
  utils::crypto::Bytes encryption_key = utils::crypto::stringToBytes(utils::StringUtils::from_hex(
      "4024b327fdc987ce3eb43dd1f690b9987e4072e0020e3edf4349ce1ad91a4e38"));
  minifi::Decryptor decryptor{utils::crypto::EncryptionProvider{encryption_key}};

  std::string encrypted_value = "l3WY1V27knTiPa6jVX0jrq4qjmKsySOu||ErntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUU5EyMloTtSytKk9a5xNKo=";
  REQUIRE(decryptor.decrypt(encrypted_value) == "CorrectHorseBatteryStaple");
}

TEST_CASE("Decryptor will throw if the value is incorrect", "[decrypt]") {
  utils::crypto::Bytes encryption_key = utils::crypto::stringToBytes(utils::StringUtils::from_hex(
      "4024b327fdc987ce3eb43dd1f690b9987e4072e0020e3edf4349ce1ad91a4e38"));
  minifi::Decryptor decryptor{utils::crypto::EncryptionProvider{encryption_key}};

  // correct nonce + ciphertext and mac: "l3WY1V27knTiPa6jVX0jrq4qjmKsySOu||ErntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUU5EyMloTtSytKk9a5xNKo="

  REQUIRE_THROWS_AS(decryptor.decrypt(  // this is not even close
      "some totally incorrect value"),
      std::exception);
  REQUIRE_THROWS_AS(decryptor.decrypt(  // separator missing
      "l3WY1V27knTiPa6jVX0jrq4qjmKsySOuErntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUU5EyMloTtSytKk9a5xNKo="),
      std::exception);
  REQUIRE_THROWS_AS(decryptor.decrypt(  // separator wrong
      "l3WY1V27knTiPa6jVX0jrq4qjmKsySOu__ErntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUU5EyMloTtSytKk9a5xNKo="),
      std::exception);
  REQUIRE_THROWS_AS(decryptor.decrypt(  // more than one separator
      "l3WY1V27knTiPa6jVX0jrq4qjmKsySOu||ErntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUU5EyMloTtSytKk9a5xNKo=||extra+stuff"),
      std::exception);
  REQUIRE_THROWS_AS(decryptor.decrypt(  // nonce is off by one char
      "L3WY1V27knTiPa6jVX0jrq4qjmKsySOu||ErntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUU5EyMloTtSytKk9a5xNKo="),
      std::exception);
  REQUIRE_THROWS_AS(decryptor.decrypt(  // ciphertext is off by one char
      "l3WY1V27knTiPa6jVX0jrq4qjmKsySOu||erntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUU5EyMloTtSytKk9a5xNKo="),
      std::exception);
  REQUIRE_THROWS_AS(decryptor.decrypt(  // nonce is too short
      "l3WY1V27knTiPa6jVX0rq4qjmKsySOu||ErntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUU5EyMloTtSytKk9a5xNKo="),
      std::exception);
  REQUIRE_THROWS_AS(decryptor.decrypt(  // nonce is too long
      "l3WY1V27knTiPa6jVX0jrq4qjmKsySOup||ErntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUU5EyMloTtSytKk9a5xNKo="),
      std::exception);
  REQUIRE_THROWS_AS(decryptor.decrypt(  // ciphertext-and-mac is too short
      "l3WY1V27knTiPa6jVX0jrq4qjmKsySOu||ErntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUU5EyMloTtSytk9a5xNKo="),
      std::exception);
  REQUIRE_THROWS_AS(decryptor.decrypt(  // ciphertext-and-mac is too long
      "l3WY1V27knTiPa6jVX0jrq4qjmKsySOu||ErntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUUU5EyMloTtSytKk9a5xNKo="),
      std::exception);
  REQUIRE_THROWS_AS(decryptor.decrypt(  // correct format but random value
      "81hf/4bHIRVd2pYglniBW3zOUcaLe+Cw||mkN2sKHS+nepRTcBhOJ5tFW4GXvaywYLD8xzIEbCP0lgUA6Qf3jZ/oMi"),
      std::exception);
}

TEST_CASE("Decryptor can decrypt a configuration file", "[decryptSensitiveProperties]") {
  utils::crypto::Bytes encryption_key = utils::crypto::stringToBytes(utils::StringUtils::from_hex(
      "5506c28d0fe265299e294a4c766b723a48986764953e93d38b3c627176fd10ed"));
  minifi::Decryptor decryptor{utils::crypto::EncryptionProvider{encryption_key}};

  minifi::Configure configuration{decryptor};
  configuration.setHome("resources");
  configuration.loadConfigureFile("encrypted.minifi.properties");
  REQUIRE(configuration.getConfiguredKeys().size() > 0);

  utils::optional<std::string> passphrase = configuration.get(minifi::Configure::nifi_security_client_pass_phrase);
  REQUIRE(passphrase);
  REQUIRE(*passphrase == "SpeakFriendAndEnter");

  utils::optional<std::string> password = configuration.get(minifi::Configure::nifi_rest_api_password);
  REQUIRE(password);
  REQUIRE(*password == "OpenSesame");

  std::string agent_identifier;
  REQUIRE(configuration.get("nifi.c2.agent.identifier", "c2.agent.identifier", agent_identifier));
  REQUIRE(agent_identifier == "TailFileTester-001");

  utils::optional<std::string> unencrypted_property = configuration.get(minifi::Configure::nifi_bored_yield_duration);
  REQUIRE(unencrypted_property);
  REQUIRE(*unencrypted_property == "10 millis");

  utils::optional<std::string> nonexistent_property = configuration.get("this.property.does.not.exist");
  REQUIRE_FALSE(nonexistent_property);
}

TEST_CASE("Decryptor can be created from a bootstrap file", "[create]") {
  utils::optional<minifi::Decryptor> valid_decryptor = minifi::Decryptor::create("resources");
  REQUIRE(valid_decryptor);
  REQUIRE(valid_decryptor->decrypt("HvbPejGT3ur9/00gXQK/dJCYwaNqhopf||CiXKiNaljSN7VkLXP5zfJnb4+4UcKIG3ddwuVfSPpkRRfT4=") == "SpeakFriendAndEnter");

  utils::optional<minifi::Decryptor> invalid_decryptor = minifi::Decryptor::create("there.is.no.such.directory");
  REQUIRE_FALSE(invalid_decryptor);
}
