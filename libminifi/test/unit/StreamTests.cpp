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

#include <thread>
#include <random>
#include <chrono>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include "../TestBase.h"
#include "io/BaseStream.h"

TEST_CASE("TestReadData", "[testread]") {
  auto base = std::make_shared<minifi::io::BaseStream>();
  uint64_t b = 8;
  base->write(b);
  uint64_t c = 0;
  base->readData(reinterpret_cast<uint8_t*>(&c), 8);
  if (minifi::io::EndiannessCheck::IS_LITTLE)
    REQUIRE(c == 576460752303423488);
  else
    REQUIRE(c == 8);
}

TEST_CASE("TestRead8", "[testread]") {
  auto base = std::make_shared<minifi::io::BaseStream>();
  uint64_t b = 8;
  base->write(b);
  uint64_t c = 0;
  base->read(c);
  REQUIRE(c == 8);
}

TEST_CASE("TestRead2", "[testread]") {
  auto base = std::make_shared<minifi::io::BaseStream>();
  uint16_t b = 8;
  base->write(b);
  uint16_t c = 0;
  base->read(c);
  REQUIRE(c == 8);
}

TEST_CASE("TestRead1", "[testread]") {
  auto base = std::make_shared<minifi::io::BaseStream>();
  uint8_t b = 8;
  base->write(&b, 1);
  uint8_t c = 0;
  base->read(c);
  REQUIRE(c == 8);
}

TEST_CASE("TestRead4", "[testread]") {
  auto base = std::make_shared<minifi::io::BaseStream>();
  uint32_t b = 8;
  base->write(b);
  uint32_t c = 0;
  base->read(c);
  REQUIRE(c == 8);
}
