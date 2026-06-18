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

#include "EncryptConfig.h"
#include "properties/PropertiesFile.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"

namespace {
constexpr std::string llm_api_key = "llm.api.key";
}

TEST_CASE("EncryptConfig::encryptSensitiveValuesInMinifiProperties can encrypt the sensitive properties", "[encrypt-config][encryptSensitivePropertiesInFile]") {
  TestController test_controller;
  const auto minifi_home = test_controller.createTempDirectory();
  std::filesystem::copy("resources/conf", minifi_home / "conf", std::filesystem::copy_options::recursive);

  auto original_properties = minifi::PropertiesImpl{minifi::PropertiesImpl::PersistTo::MultipleFiles, "minifi.properties"};
  original_properties.loadConfigureFile(minifi_home / "conf" / "minifi.properties");
  const auto original_passphrase = original_properties.getString(minifi::Configuration::nifi_security_client_pass_phrase);
  const auto original_password = original_properties.getString(minifi::Configuration::nifi_rest_api_password);
  const auto original_api_key = original_properties.getString(llm_api_key);
  REQUIRE(original_passphrase);
  REQUIRE(original_password);
  REQUIRE(original_api_key);

  minifi::encrypt_config::EncryptConfig encrypt_config{minifi_home};
  encrypt_config.encryptSensitiveValuesInMinifiProperties();

  auto encrypted_properties = minifi::PropertiesImpl{minifi::PropertiesImpl::PersistTo::MultipleFiles, "minifi.properties"};
  encrypted_properties.loadConfigureFile(minifi_home / "conf" / "minifi.properties");
  const auto encrypted_passphrase = encrypted_properties.getString(minifi::Configuration::nifi_security_client_pass_phrase);
  const auto encrypted_password = encrypted_properties.getString(minifi::Configuration::nifi_rest_api_password);
  const auto encrypted_api_key = encrypted_properties.getString(llm_api_key);
  REQUIRE(encrypted_passphrase);
  REQUIRE(encrypted_password);
  REQUIRE(encrypted_api_key);
  CHECK(encrypted_passphrase != original_passphrase);
  CHECK(encrypted_password != original_password);
  CHECK(encrypted_api_key != original_api_key);

  minifi::PropertiesFile bootstrap_file{std::ifstream{minifi_home / "conf" / "bootstrap.conf"}};
  const auto encryption_key_hex = bootstrap_file.getValue("nifi.bootstrap.sensitive.key");
  REQUIRE(encryption_key_hex);
  const auto encryption_key = utils::string::from_hex(*encryption_key_hex);

  CHECK(utils::crypto::decrypt(*encrypted_passphrase, encryption_key) == *original_passphrase);
  CHECK(utils::crypto::decrypt(*encrypted_password, encryption_key) == *original_password);
  CHECK(utils::crypto::decrypt(*encrypted_api_key, encryption_key) == *original_api_key);
}
