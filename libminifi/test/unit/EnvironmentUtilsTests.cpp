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

#include <cstdint>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "utils/Environment.h"
#include "utils/file/PathUtils.h"
#include "minifi-cpp/utils/gsl.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

TEST_CASE("getenv already existing", "[getenv]") {
  auto res = utils::Environment::getEnvironmentVariable("PATH");
  REQUIRE(res.has_value());
  REQUIRE(!res->empty());
}

TEST_CASE("getenv not existing", "[getenv]") {
  auto res = utils::Environment::getEnvironmentVariable("GETENV1");
  REQUIRE(!res);
}

TEST_CASE("getenv empty existing", "[getenv]") {
  REQUIRE(true == utils::Environment::setEnvironmentVariable("GETENV2", ""));
  auto res = utils::Environment::getEnvironmentVariable("GETENV2");
  REQUIRE(res);
  CHECK(res->empty());
}

TEST_CASE("setenv not existing overwrite", "[setenv]") {
  REQUIRE(true == utils::Environment::setEnvironmentVariable("SETENV1", "test"));
  auto res = utils::Environment::getEnvironmentVariable("SETENV1");
  REQUIRE(res);
  CHECK("test" == *res);
}

TEST_CASE("setenv existing overwrite", "[setenv]") {
  REQUIRE(true == utils::Environment::setEnvironmentVariable("SETENV2", "test"));
  REQUIRE(true == utils::Environment::setEnvironmentVariable("SETENV2", "test2"));
  auto res = utils::Environment::getEnvironmentVariable("SETENV2");
  REQUIRE(res);
  CHECK("test2" == *res);
}

TEST_CASE("setenv not existing no overwrite", "[setenv]") {
  REQUIRE(true == utils::Environment::setEnvironmentVariable("SETENV3", "test", false /*overwrite*/));
  auto res = utils::Environment::getEnvironmentVariable("SETENV3");
  REQUIRE(res);
  CHECK("test" == *res);
}

TEST_CASE("setenv existing no overwrite", "[setenv]") {
  REQUIRE(true == utils::Environment::setEnvironmentVariable("SETENV4", "test"));
  REQUIRE(true == utils::Environment::setEnvironmentVariable("SETENV4", "test2", false /*overwrite*/));
  auto res = utils::Environment::getEnvironmentVariable("SETENV4");
  REQUIRE(res);
  CHECK("test" == *res);
}

TEST_CASE("unsetenv not existing", "[unsetenv]") {
  REQUIRE(!utils::Environment::getEnvironmentVariable("UNSETENV1"));
  REQUIRE(true == utils::Environment::unsetEnvironmentVariable("UNSETENV1"));
  REQUIRE(!utils::Environment::getEnvironmentVariable("UNSETENV1"));
}

TEST_CASE("unsetenv existing", "[unsetenv]") {
  REQUIRE(true == utils::Environment::setEnvironmentVariable("UNSETENV2", "test"));
  REQUIRE(utils::Environment::getEnvironmentVariable("UNSETENV2"));
  REQUIRE(true == utils::Environment::unsetEnvironmentVariable("UNSETENV2"));
  REQUIRE(!utils::Environment::getEnvironmentVariable("UNSETENV2"));
}

TEST_CASE("multithreaded environment manipulation", "[getenv][setenv][unsetenv]") {
  std::vector<std::thread> threads;
  threads.reserve(16U);
  for (size_t i = 0U; i < 16U; i++) {
    threads.emplace_back([](){
      std::mt19937 gen(std::random_device { }());
      for (size_t i = 0U; i < 10240U; i++) {
        const uint8_t env_num = gen() % 8;
        const std::string env_name = "GETSETUNSETENV" + std::to_string(env_num);
        const uint8_t operation = gen() % 3;
        switch (operation) {
          case 0: {
              auto res = utils::Environment::getEnvironmentVariable(env_name.c_str());
              break;
            }
          case 1: {
              const size_t value_len = gen() % 256;
              std::vector<char> value(value_len + 1, '\0');
              std::generate_n(value.begin(), value_len, [&]() -> char {
                return gsl::narrow<char>('A' + (gen() % static_cast<uint8_t>('Z' - 'A')));
              });
              const bool overwrite = gen() % 2;
              utils::Environment::setEnvironmentVariable(env_name.c_str(), value.data(), overwrite);
              break;
            }
          case 2: {
              utils::Environment::unsetEnvironmentVariable(env_name.c_str());
              break;
            }
          default: {
            gsl_FailFast();
          }
        }
      }
      });
    }
  for (auto& thread : threads) {
    thread.join();
  }
  for (size_t i = 0U; i < 8U; i++) {
    const std::string env_name = "GETSETUNSETENV" + std::to_string(i);
    auto value = utils::Environment::getEnvironmentVariable(env_name.c_str());
    if (value) {
      std::cerr << env_name << " is set to " << *value << std::endl;
    } else {
      std::cerr << env_name << " is not set" << std::endl;
    }
  }
}
