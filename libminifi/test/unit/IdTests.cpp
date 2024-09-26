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
#include <utility>
#include <string>
#include <memory>
#include <ctime>
#include <algorithm>
#include <cctype>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/Id.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::utils {
struct IdentifierTestAccessor {
  FIELD_ACCESSOR(data_)
};
}  // namespace org::apache::nifi::minifi::utils

using org::apache::nifi::minifi::utils::IdentifierTestAccessor;

TEST_CASE("Test default is time", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(std::make_shared<minifi::PropertiesImpl>());

  REQUIRE(true == LogTestController::getInstance().contains("Using uuid_generate_time implementation for uids."));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test time", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  id_props->set("uid.implementation", "TiMe");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  REQUIRE(true == LogTestController::getInstance().contains("Using uuid_generate_time implementation for uids."));

  utils::Identifier id = generator->generate();

  uint8_t version = IdentifierTestAccessor::get_data_(id)[6] >> 4;
  REQUIRE(0x01 == version);

  LogTestController::getInstance().reset();
}

TEST_CASE("Test Generate Move", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  id_props->set("uid.implementation", "TiMe");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  auto generated = generator->generate();
  auto str = generated.to_string();
  utils::Identifier moved = std::move(generated);  // NOLINT(performance-move-const-arg)
  auto str2 = moved.to_string();
  REQUIRE(str == str2);
}

TEST_CASE("Test random", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  id_props->set("uid.implementation", "RaNDoM");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  REQUIRE(true == LogTestController::getInstance().contains("Using uuid_generate_random for uids."));

  utils::Identifier id = generator->generate();

  uint8_t version = IdentifierTestAccessor::get_data_(id)[6] >> 4;
  REQUIRE(0x04 == version);

  LogTestController::getInstance().reset();
}

TEST_CASE("Test uuid_default", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  id_props->set("uid.implementation", "UUID_default");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  REQUIRE(true == LogTestController::getInstance().contains("Using uuid_generate for uids."));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test invalid", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  id_props->set("uid.implementation", "InVaLiD");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  REQUIRE(true == LogTestController::getInstance().contains("Invalid value for uid.implementation (invalid). Using uuid_generate_time implementation for uids."));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test parse", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  id_props->set("uid.implementation", "time");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  const std::map<std::string, std::array<uint8_t, 16U>> test_cases = {
      {"00000000-0000-0000-0000-000000000000", {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
      {"1d412e16-0148-11ea-880b-9bf2c1d8f5be", {0x1D, 0x41, 0x2E, 0x16, 0x01, 0x48, 0x11, 0xEA, 0x88, 0x0B, 0x9B, 0xF2, 0xC1, 0xD8, 0xF5, 0xBE}},
      {"d85b49a4-32dc-42de-a26a-c2eb6222118c", {0xD8, 0x5B, 0x49, 0xA4, 0x32, 0xDC, 0x42, 0xDE, 0xA2, 0x6A, 0xC2, 0xEB, 0x62, 0x22, 0x11, 0x8C}},
      {"CEF85A08-0148-11EA-97D5-93123D0B5F8A", {0xCE, 0xF8, 0x5A, 0x08, 0x01, 0x48, 0x11, 0xEA, 0x97, 0xD5, 0x93, 0x12, 0x3D, 0x0B, 0x5F, 0x8A}},
  };

  for (const auto& test_case : test_cases) {
    utils::Identifier id = utils::Identifier::parse(test_case.first).value();
    REQUIRE(memcmp(IdentifierTestAccessor::get_data_(id).data(), test_case.second.data(), 16U) == 0);
    REQUIRE(utils::string::equalsIgnoreCase(test_case.first, id.to_string().view()));
  }

  LogTestController::getInstance().reset();
}

TEST_CASE("Test parse invalid", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  id_props->set("uid.implementation", "time");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  const std::map<std::string, bool> test_cases{
    {"12", false},  // drastically shorter
    {"12345678-1234-1234-1234-123456789abc", true},  // ok
    {"12345678-1234-1234-1234-123456789ab", false},  // shorter by one
    {"12345678-1234-1234-1234-123456789abc0", false},  // longer by one ('0')
    {"123456789-123-1234-1234-123456789abc", false},  // first block is longer, but second shorter
    {"12345678-1234-1234-1234-123456789abZ", false},  // contains invalid character at the end
    {"123456780123401234012340123456789abc", false},  // missing delimiters
    {"12345678-12-34-1234-1234-123456789ab", false},  // exra hyphen
    {"utter garbage but exactly 36 chars  ", false}  // utter garbage
  };

  for (const auto& it : test_cases) {
    REQUIRE(static_cast<bool>(utils::Identifier::parse(it.first)) == it.second);
  }
}

TEST_CASE("Test to_string", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  id_props->set("uid.implementation", "time");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  utils::Identifier id = generator->generate();

  std::string id_str = id.to_string();
  std::cerr << "Generated UUID " << id_str << std::endl;

  REQUIRE(36 == id_str.length());
  REQUIRE('-' == id_str[8]);
  REQUIRE('-' == id_str[13]);
  REQUIRE('-' == id_str[18]);
  REQUIRE('-' == id_str[23]);
  for (size_t i : {0, 1, 2, 3, 4, 5, 6, 7}) {
    REQUIRE(isxdigit(id_str[i]));
  }
  for (size_t i : {9, 10, 11, 12}) {
    REQUIRE(isxdigit(id_str[i]));
  }
  for (size_t i : {14, 15, 16, 17}) {
    REQUIRE(isxdigit(id_str[i]));
  }
  for (size_t i : {14, 15, 16, 17}) {
    REQUIRE(isxdigit(id_str[i]));
  }
  for (size_t i : {19, 20, 21, 22}) {
    REQUIRE(isxdigit(id_str[i]));
  }
  for (size_t i : {24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35}) {
    REQUIRE(isxdigit(id_str[i]));
  }

  LogTestController::getInstance().reset();
}

TEST_CASE("Test Hex Device Segment 16 bits correct digits", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  id_props->set("uid.implementation", "minifi_uid");
  id_props->set("uid.minifi.device.segment", "09aF");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  utils::Identifier uuid = generator->generate();
  auto& data = IdentifierTestAccessor::get_data_(uuid);
  REQUIRE(0x09 == data[0]);
  REQUIRE(0xaf == data[1]);
  REQUIRE(0 == data[15]);

  uuid = generator->generate();
  REQUIRE(0x09 == data[0]);
  REQUIRE(0xaf == data[1]);
  REQUIRE(1 == data[15]);

  REQUIRE(true == LogTestController::getInstance().contains("Using user defined device segment: 0x9af"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test Hex Device Segment 16 bits too many digits", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  id_props->set("uid.implementation", "minifi_uid");
  id_props->set("uid.minifi.device.segment", "09aFee");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  utils::Identifier uuid = generator->generate();
  auto& data = IdentifierTestAccessor::get_data_(uuid);
  REQUIRE(0x09 == data[0]);
  REQUIRE(0xaf == data[1]);
  REQUIRE(0 == (data[2] & 128));
  REQUIRE(0 == data[15]);

  uuid = generator->generate();
  REQUIRE(0x09 == data[0]);
  REQUIRE(0xaf == data[1]);
  REQUIRE(0 == (data[2] & 128));
  REQUIRE(1 == data[15]);

  REQUIRE(true == LogTestController::getInstance().contains("Using user defined device segment: 0x9af"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test Hex Device Segment 18 bits", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  id_props->set("uid.implementation", "minifi_uid");
  id_props->set("uid.minifi.device.segment.bits", "18");
  id_props->set("uid.minifi.device.segment", "09aF8");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  utils::Identifier uuid = generator->generate();
  auto& data = IdentifierTestAccessor::get_data_(uuid);
  REQUIRE(0x09 == data[0]);
  REQUIRE(0xaf == data[1]);
  REQUIRE(128 == (data[2] & 192));
  REQUIRE(0 == data[15]);

  uuid = generator->generate();
  REQUIRE(0x09 == data[0]);
  REQUIRE(0xaf == data[1]);
  REQUIRE(128 == (data[2] & 192));
  REQUIRE(1 == data[15]);

  utils::Identifier uuid2 = generator->generate();
  REQUIRE(uuid.to_string() != uuid2.to_string());
  REQUIRE(uuid != uuid2);

  REQUIRE(true == LogTestController::getInstance().contains("Using minifi uid prefix: 0x9af8"));
  LogTestController::getInstance().reset();
}


TEST_CASE("Collision", "[collision]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  SECTION("random") {
    id_props->set("uid.implementation", "random");
  }
  SECTION("time") {
    id_props->set("uid.implementation", "time");
  }
  SECTION("uuid_default") {
    id_props->set("uid.implementation", "uuid_default");
  }

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  std::vector<utils::Identifier> uuids(16 * 1024U);
  std::vector<std::thread> threads;
  for (size_t i = 0U; i < 16U; i++) {
    threads.emplace_back([&generator, &uuids, i](){
      for (size_t j = 0U; j < 1024U; j++) {
        uuids[i * 1024U + j] = generator->generate();
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }

  std::sort(uuids.begin(), uuids.end(), [](const utils::Identifier& a, const utils::Identifier& b) {
    return memcmp(IdentifierTestAccessor::get_data_(a).data(), IdentifierTestAccessor::get_data_(b).data(), 16U) < 0;
  });
  REQUIRE(uuids.end() == std::adjacent_find(uuids.begin(), uuids.end()));

  LogTestController::getInstance().reset();
}

TEST_CASE("Speed", "[speed]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::PropertiesImpl>();
  std::string implementation;
  SECTION("random") {
    implementation = "random";
  }
  SECTION("time") {
    implementation = "time";
  }
  SECTION("uuid_default") {
    implementation = "uuid_default";
  }
  id_props->set("uid.implementation", implementation);

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  std::vector<utils::Identifier> uuids(128U * 1024U);
  // Prime the generator
  generator->generate();

  auto before = std::chrono::high_resolution_clock::now();
  for (auto& uuid : uuids) {
    uuid = generator->generate();
  }
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - before).count();
  std::cerr << "Generating one " << implementation << " UUID took " << (duration / uuids.size()) << "ns" << std::endl;

  LogTestController::getInstance().reset();
}
