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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace encrypt_config {

class ConfigFileTestAccessor {
 public:
  static std::vector<std::string> mergeProperties(std::vector<std::string> left, const std::vector<std::string>& right) {
    return ConfigFile::mergeProperties(left, right);
  }
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
    REQUIRE_THROWS(test_file.update("nifi.bored.panda", "cat video"));
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
    REQUIRE_THROWS(test_file.insertAfter("nifi.toil.api.password", "key", "value"));
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
    REQUIRE(test_file.getSensitiveProperties() == expected_properties);
  }

  SECTION("with additional properties") {
    ConfigFile test_file{"resources/with-additional-sensitive-props.minifi.properties"};
    std::vector<std::string> expected_properties{
        "nifi.c2.enable", "nifi.flow.configuration.file", "nifi.rest.api.password", "nifi.security.client.pass.phrase"};
    REQUIRE(test_file.getSensitiveProperties() == expected_properties);
  }
}
