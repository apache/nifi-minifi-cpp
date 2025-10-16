/**
 *
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
#include <unordered_set>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "properties/Configuration.h"
#include "utils/Environment.h"

namespace {
bool fileContentsMatch(const std::filesystem::path& file_name, const std::unordered_set<std::string>& expected_contents) {
  std::unordered_set<std::string> actual_contents;
  std::ifstream file{file_name};
  if (file.is_open()) {
    std::string line;
    while (std::getline(file, line)) {
      actual_contents.insert(line);
    }
  }
  return expected_contents == actual_contents;
}
}  // namespace

namespace org::apache::nifi::minifi::test {

TEST_CASE("Configuration can merge lists of property names", "[mergeProperties]") {
  using vector = std::vector<std::string>;

  REQUIRE(Configuration::mergeProperties(vector{}, vector{}) == vector{});  // NOLINT(readability-container-size-empty)

  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{}) == vector{"a"});
  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{"a"}) == vector{"a"});
  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{"b"}) == (vector{"a", "b"}));

  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"c"}) == (vector{"a", "b", "c"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"a", "b"}) == (vector{"a", "b"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"a", "c"}) == (vector{"a", "b", "c"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"b", "c"}) == (vector{"a", "b", "c"}));

  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{" a"}) == vector{"a"});
  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{"a "}) == vector{"a"});
  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{" a "}) == vector{"a"});

  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"\tc"}) == (vector{"a", "b", "c"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"a\n", "b"}) == (vector{"a", "b"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"a", "c\r\n"}) == (vector{"a", "b", "c"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"b\n", "\t c"}) == (vector{"a", "b", "c"}));
}

TEST_CASE("Configuration can validate values to be assigned to specific properties", "[validatePropertyValue]") {
  REQUIRE(Configuration::validatePropertyValue(Configuration::nifi_c2_agent_identifier_fallback, "anything is valid"));
  REQUIRE_FALSE(Configuration::validatePropertyValue(Configuration::nifi_flow_configuration_encrypt, "invalid.value"));
  REQUIRE(Configuration::validatePropertyValue(Configuration::nifi_flow_configuration_encrypt, "true"));
  REQUIRE(Configuration::validatePropertyValue("random.property", "random_value"));
}

TEST_CASE("Configuration can fix misconfigured timeperiod<->integer validated properties") {
  LogTestController::getInstance().setInfo<minifi::Configure>();
  LogTestController::getInstance().setInfo<minifi::Properties>();

  TestController test_controller;
  const auto conf_directory = test_controller.createTempDirectory();
  const auto original_properties_path = conf_directory / "test.properties";
  const auto updated_properties_path = conf_directory / "test.properties.d" / PropertiesImpl::C2PropertiesFileName;

  std::ofstream{original_properties_path}
      << "nifi.c2.agent.heartbeat.period=1min\n"
      << "nifi.administrative.yield.duration=30000\n";
  auto properties_file_time_after_creation = std::filesystem::last_write_time(original_properties_path);
  const std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::ConfigureImpl>();

  configure->loadConfigureFile(original_properties_path);
  CHECK(configure->get("nifi.c2.agent.heartbeat.period") == "60000");
  CHECK(configure->get("nifi.administrative.yield.duration") == "30000 ms");

  {
    CHECK(properties_file_time_after_creation == std::filesystem::last_write_time(original_properties_path));
    CHECK(fileContentsMatch(original_properties_path, {"nifi.c2.agent.heartbeat.period=1min", "nifi.administrative.yield.duration=30000"}));
  }

  CHECK(configure->commitChanges());

  {
    CHECK(properties_file_time_after_creation == std::filesystem::last_write_time(original_properties_path));
    CHECK(properties_file_time_after_creation <= std::filesystem::last_write_time(updated_properties_path));
    CHECK(fileContentsMatch(updated_properties_path, {"nifi.c2.agent.heartbeat.period=60000", "nifi.administrative.yield.duration=30000 ms"}));
  }

  const std::shared_ptr<minifi::Configure> configure_reread = std::make_shared<minifi::ConfigureImpl>();
  configure_reread->loadConfigureFile(original_properties_path);
  CHECK(configure_reread->get("nifi.c2.agent.heartbeat.period") == "60000");
  CHECK(configure_reread->get("nifi.administrative.yield.duration") == "30000 ms");
}

TEST_CASE("Configuration can fix misconfigured datasize<->integer validated properties") {
  LogTestController::getInstance().setInfo<minifi::Configure>();
  LogTestController::getInstance().setInfo<minifi::Properties>();

  TestController test_controller;
  const auto conf_directory = test_controller.createTempDirectory();
  const auto original_properties_path = conf_directory / "test.properties";
  const auto updated_properties_path = conf_directory / "test.properties.d" / PropertiesImpl::C2PropertiesFileName;

  {
    std::ofstream properties_file(original_properties_path);
    properties_file << "appender.rolling.max_file_size=6000" << std::endl;
    properties_file.close();
  }
  auto properties_file_time_after_creation = std::filesystem::last_write_time(original_properties_path);
  const std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::ConfigureImpl>();
  configure->loadConfigureFile(original_properties_path, "nifi.log.");
  CHECK(configure->get("appender.rolling.max_file_size") == "6000 B");

  {
    CHECK(properties_file_time_after_creation == std::filesystem::last_write_time(original_properties_path));
    CHECK(fileContentsMatch(original_properties_path, {"appender.rolling.max_file_size=6000"}));
  }

  CHECK(configure->commitChanges());

  {
    CHECK(properties_file_time_after_creation == std::filesystem::last_write_time(original_properties_path));
    CHECK(properties_file_time_after_creation <= std::filesystem::last_write_time(updated_properties_path));
    CHECK(fileContentsMatch(updated_properties_path, {"appender.rolling.max_file_size=6000 B"}));
  }

  const std::shared_ptr<minifi::Configure> configure_reread = std::make_shared<minifi::ConfigureImpl>();
  configure_reread->loadConfigureFile(original_properties_path, "nifi.log.");
  CHECK(configure_reread->get("appender.rolling.max_file_size") == "6000 B");
}

TEST_CASE("Configuration can fix misconfigured validated properties within environmental variables") {
  LogTestController::getInstance().setInfo<minifi::Configure>();
  LogTestController::getInstance().setInfo<minifi::Properties>();

  TestController test_controller;
  const auto conf_directory = test_controller.createTempDirectory();
  const auto original_properties_path = conf_directory / "test.properties";
  const auto updated_properties_path = conf_directory / "test.properties.d" / PropertiesImpl::C2PropertiesFileName;

  CHECK(minifi::utils::Environment::setEnvironmentVariable("SOME_VARIABLE", "4000"));

  std::ofstream{original_properties_path}
      << "compression.cached.log.max.size=${SOME_VARIABLE}\n"
      << "compression.compressed.log.max.size=3000\n";
  auto properties_file_time_after_creation = std::filesystem::last_write_time(original_properties_path);
  const std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::ConfigureImpl>();

  configure->loadConfigureFile(original_properties_path, "nifi.log.");
  CHECK(configure->get("compression.cached.log.max.size") == "4000 B");
  CHECK(configure->get("compression.compressed.log.max.size") == "3000 B");

  {
    CHECK(properties_file_time_after_creation == std::filesystem::last_write_time(original_properties_path));
    CHECK(fileContentsMatch(original_properties_path, {"compression.cached.log.max.size=${SOME_VARIABLE}", "compression.compressed.log.max.size=3000"}));
  }

  CHECK(configure->commitChanges());

  {
    CHECK(properties_file_time_after_creation == std::filesystem::last_write_time(original_properties_path));
    CHECK(properties_file_time_after_creation <= std::filesystem::last_write_time(updated_properties_path));
    CHECK(fileContentsMatch(updated_properties_path, {"compression.compressed.log.max.size=3000 B"}));
  }

  const std::shared_ptr<minifi::Configure> configure_reread = std::make_shared<minifi::ConfigureImpl>();
  configure_reread->loadConfigureFile(original_properties_path, "nifi.log.");
  CHECK(configure_reread->get("compression.cached.log.max.size") == "4000 B");
  CHECK(configure_reread->get("compression.compressed.log.max.size") == "3000 B");
}

TEST_CASE("Committing changes to a configuration creates a backup file") {
  LogTestController::getInstance().setInfo<minifi::Configure>();
  LogTestController::getInstance().setInfo<minifi::Properties>();

  TestController test_controller;
  const auto conf_directory = test_controller.createTempDirectory();
  const auto original_properties_path = conf_directory / "test.properties";
  const auto updated_properties_path = conf_directory / "test.properties.d" / PropertiesImpl::C2PropertiesFileName;
  const auto backup_properties_path = [&]() {
    auto path = updated_properties_path;
    path += ".bak";
    return path;
  }();

  std::ofstream{original_properties_path}
      << "number.of.lions=7\n"
      << "number.of.elephants=12\n"
      << "number.of.giraffes=30\n";

  const std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::ConfigureImpl>();
  configure->loadConfigureFile(original_properties_path);

  CHECK(configure->get("number.of.lions") == "7");
  CHECK(configure->get("number.of.elephants") == "12");
  CHECK(configure->get("number.of.giraffes") == "30");

  configure->set("number.of.lions", "8");
  CHECK(configure->commitChanges());
  CHECK(fileContentsMatch(updated_properties_path, {"number.of.lions=8"}));
  CHECK_FALSE(std::filesystem::exists(backup_properties_path));

  const std::shared_ptr<minifi::Configure> configure_2 = std::make_shared<minifi::ConfigureImpl>();
  configure_2->loadConfigureFile(original_properties_path);
  CHECK(configure_2->get("number.of.lions") == "8");
  CHECK(configure_2->get("number.of.elephants") == "12");
  CHECK(configure_2->get("number.of.giraffes") == "30");

  configure->set("number.of.giraffes", "29");
  CHECK(configure->commitChanges());
  CHECK(fileContentsMatch(updated_properties_path, {"number.of.lions=8", "number.of.giraffes=29"}));
  CHECK(fileContentsMatch(backup_properties_path, {"number.of.lions=8"}));

  const std::shared_ptr<minifi::Configure> configure_3 = std::make_shared<minifi::ConfigureImpl>();
  configure_3->loadConfigureFile(original_properties_path);
  CHECK(configure_3->get("number.of.lions") == "8");
  CHECK(configure_3->get("number.of.elephants") == "12");
  CHECK(configure_3->get("number.of.giraffes") == "29");
}

TEST_CASE("Backup file are skipped when reading config files") {
  LogTestController::getInstance().setInfo<minifi::Configure>();
  LogTestController::getInstance().setInfo<minifi::Properties>();

  TestController test_controller;
  const auto conf_directory = test_controller.createTempDirectory();
  const auto original_properties_path = conf_directory / "test.properties";
  const auto updated_properties_path = conf_directory / "test.properties.d" / PropertiesImpl::C2PropertiesFileName;
  const auto backup_properties_path = [&]() {
    auto path = updated_properties_path;
    path += ".bak";
    return path;
  }();

  std::ofstream{original_properties_path}
      << "number.of.lions=7\n"
      << "number.of.elephants=12\n";

  utils::file::create_dir(updated_properties_path.parent_path());
  std::ofstream{updated_properties_path}
      << "number.of.lions=8\n";

  std::ofstream{backup_properties_path}
      << "number.of.elephants=20\n"
      << "number.of.giraffes=30\n";

  const std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::ConfigureImpl>();
  configure->loadConfigureFile(original_properties_path);

  CHECK(configure->get("number.of.lions") == "8");
  CHECK(configure->get("number.of.elephants") == "12");
  CHECK_FALSE(configure->get("number.of.giraffes"));
}

}  // namespace org::apache::nifi::minifi::test
