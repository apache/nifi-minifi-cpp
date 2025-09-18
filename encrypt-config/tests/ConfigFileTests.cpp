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
#include <vector>

#include "ConfigFile.h"
#include "properties/Configuration.h"

#include "minifi-cpp/utils/gsl.h"

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/file/FileUtils.h"

using org::apache::nifi::minifi::encrypt_config::ConfigFile;
using org::apache::nifi::minifi::Configuration;

TEST_CASE("ConfigLine can be constructed from a line", "[encrypt-config][constructor]") {
  auto line_is_parsed_correctly = [](const std::string& line, const std::string& expected_key, const std::string& expected_value) {
    ConfigFile::Line config_line{line};
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
    ConfigFile::Line config_line{key, value};
    return config_line.getLine() == expected_line;
  };

  REQUIRE(can_construct_from_kv("nifi.some.key", "", "nifi.some.key="));
  REQUIRE(can_construct_from_kv("nifi.some.key", "some_value", "nifi.some.key=some_value"));
}

TEST_CASE("ConfigLine can update the value", "[encrypt-config][updateValue]") {
  auto can_update_value = [](const std::string& original_line, const std::string& new_value, const std::string& expected_line) {
    ConfigFile::Line config_line{original_line};
    config_line.updateValue(new_value);
    return config_line.getLine() == expected_line;
  };

  REQUIRE(can_update_value("nifi.some.key=some_value", "new_value", "nifi.some.key=new_value"));
  REQUIRE(can_update_value("nifi.some.key=", "new_value", "nifi.some.key=new_value"));
  REQUIRE(can_update_value("nifi.some.key=some_value", "", "nifi.some.key="));
  REQUIRE(can_update_value("nifi.some.key=some_value", "very_long_new_value", "nifi.some.key=very_long_new_value"));

  // whitespace is preserved in the key, but not in the value
  REQUIRE(can_update_value("nifi.some.key= some_value", "some_value", "nifi.some.key=some_value"));
  REQUIRE(can_update_value("nifi.some.key = some_value  ", "some_value", "nifi.some.key =some_value"));
  REQUIRE(can_update_value("  nifi.some.key=some_value", "some_value", "  nifi.some.key=some_value"));
  REQUIRE(can_update_value("  nifi.some.key =\tsome_value\r", "some_value", "  nifi.some.key =some_value"));
}

TEST_CASE("ConfigFile creates an empty object from a nonexistent file", "[encrypt-config][constructor]") {
  ConfigFile test_file{std::ifstream{"resources/nonexistent-minifi.properties"}};
  REQUIRE(test_file.size() == 0);
}

TEST_CASE("ConfigFile can parse a simple config file", "[encrypt-config][constructor]") {
  ConfigFile test_file{std::ifstream{"resources/minifi.properties"}};
  REQUIRE(test_file.size() == 109);
}

TEST_CASE("ConfigFile can test whether a key is present", "[encrypt-config][hasValue]") {
  ConfigFile test_file{std::ifstream{"resources/minifi.properties"}};
  REQUIRE(test_file.hasValue(Configuration::nifi_c2_flow_id));  // present but blank
  REQUIRE(!test_file.hasValue(Configuration::nifi_remote_input_secure));  // commented out
  REQUIRE(!test_file.hasValue("nifi.this.property.does.not.exist"));
}

TEST_CASE("ConfigFile can read empty properties correctly", "[encrypt-config][constructor]") {
  ConfigFile test_file{std::ifstream{"resources/with-additional-sensitive-props.minifi.properties"}};
  REQUIRE(test_file.size() == 105);

  auto empty_property = test_file.getValue(Configuration::nifi_security_need_ClientAuth);
  REQUIRE(empty_property);
  REQUIRE(empty_property->empty());

  auto whitespace_property = test_file.getValue(Configuration::nifi_security_client_certificate);  // value = " \t\r"
  REQUIRE(whitespace_property);
  REQUIRE(whitespace_property->empty());
}

TEST_CASE("ConfigFile can find the value for a key", "[encrypt-config][getValue]") {
  ConfigFile test_file{std::ifstream{"resources/minifi.properties"}};

  SECTION("valid key") {
    REQUIRE(test_file.getValue(Configuration::nifi_bored_yield_duration) == "10 millis");
  }

  SECTION("nonexistent key") {
    REQUIRE(test_file.getValue("nifi.bored.panda") == std::nullopt);
  }
}

TEST_CASE("ConfigFile can update the value for a key", "[encrypt-config][update]") {
  ConfigFile test_file{std::ifstream{"resources/minifi.properties"}};

  SECTION("valid key") {
    test_file.update(Configuration::nifi_bored_yield_duration, "20 millis");
    REQUIRE(test_file.getValue(Configuration::nifi_bored_yield_duration) == "20 millis");
  }

  SECTION("nonexistent key") {
    REQUIRE_THROWS(test_file.update("nifi.bored.panda", "cat video"));
  }
}

TEST_CASE("ConfigFile can add a new setting after an existing setting", "[encrypt-config][insertAfter]") {
  ConfigFile test_file{std::ifstream{"resources/minifi.properties"}};

  SECTION("valid key") {
    test_file.insertAfter(Configuration::nifi_rest_api_password, "nifi.rest.api.password.protected", "my-cipher-name");
    REQUIRE(test_file.size() == 110);
    REQUIRE(test_file.getValue("nifi.rest.api.password.protected") == "my-cipher-name");
  }

  SECTION("nonexistent key") {
    REQUIRE_THROWS(test_file.insertAfter("nifi.toil.api.password", "key", "value"));
  }
}

TEST_CASE("ConfigFile can add a new setting at the end", "[encrypt-config][append]") {
  ConfigFile test_file{std::ifstream{"resources/minifi.properties"}};

  const std::string KEY = "nifi.bootstrap.sensitive.key";
  const std::string VALUE = "aa411f289c91685ef9d5a9e5a4fad9393ff4c7a78ab978484323488caed7a9ab";
  test_file.append(KEY, VALUE);
  REQUIRE(test_file.size() == 110);
  REQUIRE(test_file.getValue(KEY) == std::make_optional(VALUE));
}

TEST_CASE("ConfigFile can write to a new file", "[encrypt-config][writeTo]") {
  ConfigFile test_file{std::ifstream{"resources/minifi.properties"}};
  test_file.update(Configuration::nifi_bored_yield_duration, "20 millis");

  TestController test_controller;
  auto temp_dir = test_controller.createTempDirectory();
  auto remove_directory = minifi::gsl::finally([&temp_dir]() { utils::file::delete_dir(temp_dir); });
  auto file_path = temp_dir / "minifi.properties";

  test_file.writeTo(file_path);

  ConfigFile test_file_copy{std::ifstream{file_path}};
  REQUIRE(test_file.size() == test_file_copy.size());
  REQUIRE(test_file_copy.getValue(Configuration::nifi_bored_yield_duration) == "20 millis");
}

TEST_CASE("ConfigFile will throw if we try to write to an invalid file name", "[encrypt-config][writeTo]") {
  ConfigFile test_file{std::ifstream{"resources/minifi.properties"}};
  const char* file_path = "/tmp/3915913c-b37d-4adc-b6a8-b8e36e44c639/6ede949c-12b3-4a91-8956-71bc6ab6f73e/some.file";
  REQUIRE_THROWS(test_file.writeTo(file_path));
}

TEST_CASE("ConfigFile can find the list of sensitive properties", "[encrypt-config][getSensitiveProperties]") {
  SECTION("default properties") {
    ConfigFile test_file{std::ifstream{"resources/minifi.properties"}};
    std::vector<std::string> expected_properties{Configuration::nifi_rest_api_password};
    REQUIRE(test_file.getSensitiveProperties() == expected_properties);
  }

  SECTION("with additional properties") {
    ConfigFile test_file{std::ifstream{"resources/with-additional-sensitive-props.minifi.properties"}};
    std::vector<std::string> expected_properties{
        Configuration::nifi_c2_enable, Configuration::nifi_flow_configuration_file,
        Configuration::nifi_rest_api_password, Configuration::nifi_security_client_pass_phrase};
    REQUIRE(test_file.getSensitiveProperties() == expected_properties);
  }
}
