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

#include <fstream>
#include <optional>
#include <string>

#include "ConfigFileEncryptor.h"
#include "properties/Configuration.h"
#include "utils/RegexUtils.h"

#include "TestBase.h"
#include "Catch.h"

using org::apache::nifi::minifi::encrypt_config::ConfigFile;
using org::apache::nifi::minifi::encrypt_config::encryptSensitivePropertiesInFile;
using org::apache::nifi::minifi::Configuration;
namespace utils = org::apache::nifi::minifi::utils;

namespace {
size_t base64_length(size_t unencoded_length) {
  return (unencoded_length + 2) / 3 * 4;
}

bool check_encryption(const ConfigFile& test_file, const std::string& property_name, size_t original_value_length) {
  const auto encrypted_value = test_file.getValue(property_name);
  if (!encrypted_value) { return false; }

  const auto encryption_type = test_file.getValue(property_name + ".protected");
  if (!encryption_type || *encryption_type != utils::crypto::EncryptionType::name()) { return false; }

  auto length = base64_length(utils::crypto::EncryptionType::nonceLength()) +
      utils::crypto::EncryptionType::separator().size() +
      base64_length(original_value_length + utils::crypto::EncryptionType::macLength());
  utils::Regex pattern("[0-9A-Za-z/+=|]{" + std::to_string(length) + "}");
  return utils::regexMatch(*encrypted_value, pattern);
}
}  // namespace

namespace org::apache::nifi::minifi {

// NOTE(fgerlits): these ==/!= operators are in the test file on purpose, and should not be part of production code,
// as they take a varying amount of time depending on which character in the line differs, so they would open up
// our code to timing attacks.  If you need == in production code, make sure to compare all pairs of chars/lines.
bool operator==(const PropertiesFile::Line& left, const PropertiesFile::Line& right) { return left.getLine() == right.getLine(); }

namespace encrypt_config {

bool operator!=(const ConfigFile::Line& left, const ConfigFile::Line& right) { return !(left == right); }
bool operator==(const  ConfigFile& left, const  ConfigFile& right) { return left.lines_ == right.lines_; }
bool operator!=(const  ConfigFile& left, const  ConfigFile& right) { return !(left == right); }

}  // namespace encrypt_config
}  // namespace org::apache::nifi::minifi

TEST_CASE("ConfigFileEncryptor can encrypt the sensitive properties", "[encrypt-config][encryptSensitivePropertiesInFile]") {
  utils::crypto::Bytes KEY = utils::StringUtils::from_base64("6q9u8LEDy1/CdmSBm8oSqPS/Ds5UOD2nRouP8yUoK10=");

  SECTION("default properties") {
    ConfigFile test_file{std::ifstream{"resources/minifi.properties"}};
    std::string original_password = test_file.getValue(Configuration::nifi_rest_api_password).value();

    uint32_t num_properties_encrypted = encryptSensitivePropertiesInFile(test_file, KEY);

    REQUIRE(num_properties_encrypted == 1);
    REQUIRE(test_file.size() == 110);
    REQUIRE(check_encryption(test_file, Configuration::nifi_rest_api_password, original_password.length()));

    SECTION("calling encryptSensitiveProperties a second time does nothing") {
      ConfigFile test_file_copy = test_file;

      uint32_t num_properties_encrypted = encryptSensitivePropertiesInFile(test_file, KEY);

      REQUIRE(num_properties_encrypted == 0);
      REQUIRE(test_file == test_file_copy);
    }

    SECTION("if you reset the password, it will get encrypted again") {
      test_file.update(Configuration::nifi_rest_api_password, original_password);

      SECTION("remove the .protected property") {
        int num_lines_removed = test_file.erase("nifi.rest.api.password.protected");
        REQUIRE(num_lines_removed == 1);
      }
      SECTION("change the value of the .protected property to blank") {
        test_file.update("nifi.rest.api.password.protected", "");
      }
      SECTION("change the value of the .protected property to 'plaintext'") {
        test_file.update("nifi.rest.api.password.protected", "plaintext");
      }

      uint32_t num_properties_encrypted = encryptSensitivePropertiesInFile(test_file, KEY);

      REQUIRE(num_properties_encrypted == 1);
      REQUIRE(check_encryption(test_file, Configuration::nifi_rest_api_password, original_password.length()));
    }
  }

  SECTION("with additional properties") {
    ConfigFile test_file{std::ifstream{"resources/with-additional-sensitive-props.minifi.properties"}};
    size_t original_file_size = test_file.size();

    std::string original_c2_enable = test_file.getValue(Configuration::nifi_c2_enable).value();
    std::string original_flow_config_file = test_file.getValue(Configuration::nifi_flow_configuration_file).value();
    std::string original_password = test_file.getValue(Configuration::nifi_rest_api_password).value();
    std::string original_pass_phrase = test_file.getValue(Configuration::nifi_security_client_pass_phrase).value();

    uint32_t num_properties_encrypted = encryptSensitivePropertiesInFile(test_file, KEY);

    REQUIRE(num_properties_encrypted == 4);
    REQUIRE(test_file.size() == original_file_size + 4);

    REQUIRE(check_encryption(test_file, Configuration::nifi_c2_enable, original_c2_enable.length()));
    REQUIRE(check_encryption(test_file, Configuration::nifi_flow_configuration_file, original_flow_config_file.length()));
    REQUIRE(check_encryption(test_file, Configuration::nifi_rest_api_password, original_password.length()));
    REQUIRE(check_encryption(test_file, Configuration::nifi_security_client_pass_phrase, original_pass_phrase.length()));
  }
}
