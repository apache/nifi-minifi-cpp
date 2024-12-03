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
#include "unit/TestBase.h"
#include "unit/Catch.h"

#include "properties/Configuration.h"
#include "utils/Environment.h"

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
  auto properties_path = test_controller.createTempDirectory() /  "test.properties";

  std::ofstream{properties_path}
      << "nifi.c2.agent.heartbeat.period=1min\n"
      << "nifi.administrative.yield.duration=30000\n";
  auto properties_file_time_after_creation = std::filesystem::last_write_time(properties_path);
  const std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::ConfigureImpl>();

  configure->loadConfigureFile(properties_path);
  CHECK(configure->get("nifi.c2.agent.heartbeat.period") == "60000");
  CHECK(configure->get("nifi.administrative.yield.duration") == "30000 ms");

  {
    CHECK(properties_file_time_after_creation == std::filesystem::last_write_time(properties_path));
    std::ifstream properties_file(properties_path);
    std::string first_line;
    std::string second_line;
    CHECK(std::getline(properties_file, first_line));
    CHECK(std::getline(properties_file, second_line));
    CHECK(first_line == "nifi.c2.agent.heartbeat.period=1min");
    CHECK(second_line == "nifi.administrative.yield.duration=30000");
  }

  CHECK(configure->commitChanges());

  {
    CHECK(properties_file_time_after_creation <= std::filesystem::last_write_time(properties_path));
    std::ifstream properties_file(properties_path);
    std::string first_line;
    std::string second_line;
    CHECK(std::getline(properties_file, first_line));
    CHECK(std::getline(properties_file, second_line));
    CHECK(first_line == "nifi.c2.agent.heartbeat.period=60000");
    CHECK(second_line == "nifi.administrative.yield.duration=30000 ms");
  }
}

TEST_CASE("Configuration can fix misconfigured datasize<->integer validated properties") {
  LogTestController::getInstance().setInfo<minifi::Configure>();
  LogTestController::getInstance().setInfo<minifi::Properties>();

  TestController test_controller;
  auto properties_path = test_controller.createTempDirectory() /  "test.properties";

  {
    std::ofstream properties_file(properties_path);
    properties_file << "appender.rolling.max_file_size=6000" << std::endl;
    properties_file.close();
  }
  auto properties_file_time_after_creation = std::filesystem::last_write_time(properties_path);
  const std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::ConfigureImpl>();

  configure->loadConfigureFile(properties_path, "nifi.log.");
  CHECK(configure->get("appender.rolling.max_file_size") == "6000 B");

  {
    CHECK(properties_file_time_after_creation <= std::filesystem::last_write_time(properties_path));
    std::ifstream properties_file(properties_path);
    std::string first_line;
    CHECK(std::getline(properties_file, first_line));
    CHECK(first_line == "appender.rolling.max_file_size=6000");
  }

  CHECK(configure->commitChanges());

  {
    CHECK(properties_file_time_after_creation <= std::filesystem::last_write_time(properties_path));
    std::ifstream properties_file(properties_path);
    std::string first_line;
    CHECK(std::getline(properties_file, first_line));
    CHECK(first_line == "appender.rolling.max_file_size=6000 B");
  }
}


TEST_CASE("Configuration can fix misconfigured validated properties within environmental variables") {
  LogTestController::getInstance().setInfo<minifi::Configure>();
  LogTestController::getInstance().setInfo<minifi::Properties>();
  TestController test_controller;
  auto properties_path = test_controller.createTempDirectory() /  "test.properties";

  CHECK(minifi::utils::Environment::setEnvironmentVariable("SOME_VARIABLE", "4000"));

  std::ofstream{properties_path}
      << "compression.cached.log.max.size=${SOME_VARIABLE}\n"
      << "compression.compressed.log.max.size=3000\n";
  auto properties_file_time_after_creation = std::filesystem::last_write_time(properties_path);
  const std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::ConfigureImpl>();

  configure->loadConfigureFile(properties_path, "nifi.log.");
  CHECK(configure->get("compression.cached.log.max.size") == "4000 B");
  CHECK(configure->get("compression.compressed.log.max.size") == "3000 B");

  {
    CHECK(properties_file_time_after_creation <= std::filesystem::last_write_time(properties_path));
    std::ifstream properties_file(properties_path);
    std::string first_line;
    std::string second_line;
    CHECK(std::getline(properties_file, first_line));
    CHECK(std::getline(properties_file, second_line));
    CHECK(first_line == "compression.cached.log.max.size=${SOME_VARIABLE}");
    CHECK(second_line == "compression.compressed.log.max.size=3000");
  }

  CHECK(configure->commitChanges());

  {
    CHECK(properties_file_time_after_creation <= std::filesystem::last_write_time(properties_path));
    std::ifstream properties_file(properties_path);
    std::string first_line;
    std::string second_line;
    CHECK(std::getline(properties_file, first_line));
    CHECK(std::getline(properties_file, second_line));
    CHECK(first_line == "compression.cached.log.max.size=${SOME_VARIABLE}");
    CHECK(second_line == "compression.compressed.log.max.size=3000 B");
  }
}

}  // namespace org::apache::nifi::minifi::test
