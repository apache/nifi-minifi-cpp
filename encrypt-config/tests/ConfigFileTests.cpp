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

#include "ConfigFile.h"

#include "gsl/gsl-lite.hpp"

#include "TestBase.h"
#include "utils/file/FileUtils.h"
#include "utils/RegexUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace encrypt_config {

class ConfigFileTestAccessor {
 public:
  static std::vector<std::string> getSensitiveProperties(const ConfigFile& instance) {
    return instance.getSensitiveProperties();
  }
  static std::vector<std::string> mergeProperties(std::vector<std::string> left, const std::vector<std::string>& right) {
    return ConfigFile::mergeProperties(left, right);
  }
};

size_t base64_length(size_t unencoded_length) {
  return (unencoded_length + 2) / 3 * 4;
}

bool check_encryption(const ConfigFile& test_file, const std::string& property_name, size_t original_value_length) {
    utils::optional<std::string> encrypted_value = test_file.getValue(property_name);
    if (!encrypted_value) { return false; }

    utils::optional<std::string> encryption_type = test_file.getValue(property_name + ".protected");
    if (!encryption_type || *encryption_type != "XChaCha20-Poly1305") { return false; }

    auto length = base64_length(24 /* nonce */ + original_value_length + 16 /* tag */);
    return utils::Regex::matchesFullInput("[0-9A-Za-z/+=]{" + std::to_string(length) + "}", *encrypted_value);
};

}  // namespace encrypt_config
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

using org::apache::nifi::minifi::encrypt_config::ConfigFile;
using org::apache::nifi::minifi::encrypt_config::ConfigFileTestAccessor;
using org::apache::nifi::minifi::encrypt_config::ConfigLine;
using org::apache::nifi::minifi::utils::file::FileUtils;

TEST_CASE("ConfigLine can be constructed from a line", "[encrypt-config][constructor]") {
  auto line_is_parsed_correctly = [](const std::string& line, const std::string& expected_key, const std::string& expected_value) {
    ConfigLine config_line{line};
    return config_line.getKey() == expected_key && config_line.getValue() == expected_value;
  };

  REQUIRE(line_is_parsed_correctly("", "", ""));
  REQUIRE(line_is_parsed_correctly("    \t  \r", "", ""));
  REQUIRE(line_is_parsed_correctly("#disabled.setting=foo", "", ""));
  REQUIRE(line_is_parsed_correctly("some line without an equals sign", "", ""));
  REQUIRE(line_is_parsed_correctly("=value_without_key", "", ""));
  REQUIRE(line_is_parsed_correctly("\t  =value_without_key", "", ""));

  REQUIRE(line_is_parsed_correctly("nifi.some.key=", "nifi.some.key", ""));
  REQUIRE(line_is_parsed_correctly("nifi.some.key=some_value", "nifi.some.key", "some_value"));
  REQUIRE(line_is_parsed_correctly("nifi.some.key = some_value", "nifi.some.key", "some_value"));
  REQUIRE(line_is_parsed_correctly("\tnifi.some.key\t=\tsome_value", "nifi.some.key", "some_value"));
  REQUIRE(line_is_parsed_correctly("nifi.some.key=some_value  \r", "nifi.some.key", "some_value"));
  REQUIRE(line_is_parsed_correctly("nifi.some.key=some value", "nifi.some.key", "some value"));
  REQUIRE(line_is_parsed_correctly("nifi.some.key=value=with=equals=signs=", "nifi.some.key", "value=with=equals=signs="));
}

TEST_CASE("ConfigLine can be constructed from a key-value pair", "[encrypt-config][constructor]") {
  auto can_construct_from_kv = [](const std::string& key, const std::string& value, const std::string& expected_line) {
    ConfigLine config_line{key, value};
    return config_line.getLine() == expected_line;
  };

  REQUIRE(can_construct_from_kv("nifi.some.key", "", "nifi.some.key="));
  REQUIRE(can_construct_from_kv("nifi.some.key", "some_value", "nifi.some.key=some_value"));
}

TEST_CASE("ConfigLine can update the value", "[encrypt-config][updateValue]") {
  auto can_update_value = [](const std::string& original_line, const std::string& new_value, const std::string& expected_line) {
    ConfigLine config_line{original_line};
    config_line.updateValue(new_value);
    return config_line.getLine() == expected_line;
  };

  REQUIRE(can_update_value("nifi.some.key=some_value", "new_value", "nifi.some.key=new_value"));
  REQUIRE(can_update_value("nifi.some.key=", "new_value", "nifi.some.key=new_value"));
  REQUIRE(can_update_value("nifi.some.key=some_value", "", "nifi.some.key="));
  REQUIRE(can_update_value("nifi.some.key=some_value", "very_long_new_value", "nifi.some.key=very_long_new_value"));
}

TEST_CASE("ConfigFile creates an empty object from a nonexistent file", "[encrypt-config][constructor]") {
  ConfigFile test_file{"resources/nonexistent-minifi.properties"};
  REQUIRE(test_file.size() == 0);
}

TEST_CASE("ConfigFile can parse a simple config file", "[encrypt-config][constructor]") {
  ConfigFile test_file{"resources/minifi.properties"};
  REQUIRE(test_file.size() == 101);
}

TEST_CASE("ConfigFile can read empty properties correctly", "[encrypt-config][constructor]") {
  ConfigFile test_file{"resources/with-additional-sensitive-props.minifi.properties"};
  REQUIRE(test_file.size() == 103);

  auto empty_property = test_file.getValue("nifi.security.need.ClientAuth");
  REQUIRE(empty_property);
  REQUIRE(empty_property->empty());

  auto whitespace_property = test_file.getValue("nifi.security.client.certificate");  // value = " \t\r"
  REQUIRE(whitespace_property);
  REQUIRE(whitespace_property->empty());
}

TEST_CASE("ConfigFile can find the value for a key", "[encrypt-config][getValue]") {
  ConfigFile test_file{"resources/minifi.properties"};

  SECTION("valid key") {
    REQUIRE(test_file.getValue("nifi.bored.yield.duration") == utils::optional<std::string>{"10 millis"});
  }

  SECTION("nonexistent key") {
    REQUIRE(test_file.getValue("nifi.bored.panda") == utils::nullopt);
  }
}

TEST_CASE("ConfigFile can update the value for a key", "[encrypt-config][update]") {
  ConfigFile test_file{"resources/minifi.properties"};

  SECTION("valid key") {
    test_file.update("nifi.bored.yield.duration", "20 millis");
    REQUIRE(test_file.getValue("nifi.bored.yield.duration") == utils::optional<std::string>{"20 millis"});
  }

  SECTION("nonexistent key") {
    REQUIRE_THROWS(
        test_file.update("nifi.bored.panda", "cat video");
    );
  }
}

TEST_CASE("ConfigFile can add a new setting after an existing setting", "[encrypt-config][insertAfter]") {
  ConfigFile test_file{"resources/minifi.properties"};

  SECTION("valid key") {
    test_file.insertAfter("nifi.rest.api.password", "nifi.rest.api.password.protected", "XChaCha20-Poly1305");
    REQUIRE(test_file.size() == 102);
    REQUIRE(test_file.getValue("nifi.rest.api.password.protected") == utils::optional<std::string>{"XChaCha20-Poly1305"});
  }

  SECTION("nonexistent key") {
    REQUIRE_THROWS(
      test_file.insertAfter("nifi.toil.api.password", "key", "value");
    );
  }
}

TEST_CASE("ConfigFile can add a new setting at the end", "[encrypt-config][append]") {
  ConfigFile test_file{"resources/minifi.properties"};

  const std::string KEY = "nifi.bootstrap.sensitive.key";
  const std::string VALUE = "aa411f289c91685ef9d5a9e5a4fad9393ff4c7a78ab978484323488caed7a9ab";
  test_file.append(KEY, VALUE);
  REQUIRE(test_file.size() == 102);
  REQUIRE(test_file.getValue(KEY) == utils::make_optional(VALUE));
}

TEST_CASE("ConfigFile can write to a new file", "[encrypt-config][writeTo]") {
    ConfigFile test_file{"resources/minifi.properties"};
    test_file.update("nifi.bored.yield.duration", "20 millis");

    char format[] = "/tmp/ConfigFileTests.tmp.XXXXXX";
    std::string temp_dir = FileUtils::create_temp_directory(format);
    auto remove_directory = gsl::finally([&temp_dir]() { FileUtils::delete_dir(temp_dir); });
    std::string file_path = FileUtils::concat_path(temp_dir, "minifi.properties");

    test_file.writeTo(file_path);

    ConfigFile test_file_copy{file_path};
    REQUIRE(test_file.size() == test_file_copy.size());
    REQUIRE(test_file_copy.getValue("nifi.bored.yield.duration") == utils::optional<std::string>{"20 millis"});
}

TEST_CASE("ConfigFile can merge lists of property names", "[encrypt-config][mergeProperties]") {
  using vector = std::vector<std::string>;

  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{}, vector{}) == vector{});

  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a"}, vector{}) == vector{"a"});
  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a"}, vector{"a"}) == vector{"a"});
  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a"}, vector{"b"}) == (vector{"a", "b"}));

  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a", "b"}, vector{"c"}) == (vector{"a", "b", "c"}));
  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a", "b"}, vector{"a", "b"}) == (vector{"a", "b"}));
  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a", "b"}, vector{"a", "c"}) == (vector{"a", "b", "c"}));
  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a", "b"}, vector{"b", "c"}) == (vector{"a", "b", "c"}));

  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a"}, vector{" a"}) == vector{"a"});
  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a"}, vector{"a "}) == vector{"a"});
  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a"}, vector{" a "}) == vector{"a"});

  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a", "b"}, vector{"\tc"}) == (vector{"a", "b", "c"}));
  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a", "b"}, vector{"a\n", "b"}) == (vector{"a", "b"}));
  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a", "b"}, vector{"a", "c\r\n"}) == (vector{"a", "b", "c"}));
  REQUIRE(ConfigFileTestAccessor::mergeProperties(vector{"a", "b"}, vector{"b\n", "\t c"}) == (vector{"a", "b", "c"}));
}

TEST_CASE("ConfigFile can find the list of sensitive properties", "[encrypt-config][getSensitiveProperties]") {
  SECTION("default properties") {
    ConfigFile test_file{"resources/minifi.properties"};
    std::vector<std::string> expected_properties{"nifi.rest.api.password"};
    REQUIRE(ConfigFileTestAccessor::getSensitiveProperties(test_file) == expected_properties);
  }

  SECTION("with additional properties") {
    ConfigFile test_file{"resources/with-additional-sensitive-props.minifi.properties"};
    std::vector<std::string> expected_properties{
        "nifi.c2.enable", "nifi.flow.configuration.file", "nifi.rest.api.password", "nifi.security.client.pass.phrase"};
    REQUIRE(ConfigFileTestAccessor::getSensitiveProperties(test_file) == expected_properties);
  }
}

TEST_CASE("ConfigFile can encrypt the sensitive properties", "[encrypt-config][encryptSensitiveProperties]") {
  utils::crypto::Bytes KEY = utils::crypto::stringToBytes(utils::StringUtils::from_base64(
      "6q9u8LEDy1/CdmSBm8oSqPS/Ds5UOD2nRouP8yUoK10="));

  SECTION("default properties") {
    ConfigFile test_file{"resources/minifi.properties"};
    std::string original_password = test_file.getValue("nifi.rest.api.password").value();

    int num_properties_encrypted = test_file.encryptSensitiveProperties(KEY);

    REQUIRE(num_properties_encrypted == 1);
    REQUIRE(test_file.size() == 102);
    REQUIRE(check_encryption(test_file, "nifi.rest.api.password", original_password.length()));

    SECTION("calling encryptSensitiveProperties a second time does nothing") {
      ConfigFile test_file_copy = test_file;

      int num_properties_encrypted = test_file.encryptSensitiveProperties(KEY);

      REQUIRE(num_properties_encrypted == 0);
      REQUIRE(test_file == test_file_copy);
    }

    SECTION("if you reset the password, it will get encrypted again") {
      test_file.update("nifi.rest.api.password", original_password);

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

      int num_properties_encrypted = test_file.encryptSensitiveProperties(KEY);

      REQUIRE(num_properties_encrypted == 1);
      REQUIRE(check_encryption(test_file, "nifi.rest.api.password", original_password.length()));
    }
  }

  SECTION("with additional properties") {
    ConfigFile test_file{"resources/with-additional-sensitive-props.minifi.properties"};
    size_t original_file_size = test_file.size();

    std::string original_c2_enable = test_file.getValue("nifi.c2.enable").value();
    std::string original_flow_config_file = test_file.getValue("nifi.flow.configuration.file").value();
    std::string original_password = test_file.getValue("nifi.rest.api.password").value();
    std::string original_pass_phrase = test_file.getValue("nifi.security.client.pass.phrase").value();

    int num_properties_encrypted = test_file.encryptSensitiveProperties(KEY);

    REQUIRE(num_properties_encrypted == 4);
    REQUIRE(test_file.size() == original_file_size + 4);

    REQUIRE(check_encryption(test_file, "nifi.c2.enable", original_c2_enable.length()));
    REQUIRE(check_encryption(test_file, "nifi.flow.configuration.file", original_flow_config_file.length()));
    REQUIRE(check_encryption(test_file, "nifi.rest.api.password", original_password.length()));
    REQUIRE(check_encryption(test_file, "nifi.security.client.pass.phrase", original_pass_phrase.length()));
  }
}
