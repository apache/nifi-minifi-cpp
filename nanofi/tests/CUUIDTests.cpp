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

#include <string>
#include <cstring>
#include <thread>
#include "TestBase.h"
#include "core/cuuid.h"

bool verify_uuid(const char * uuid) {
  std::string uuid_str(uuid, 36);
  fprintf(stderr, "Verifying UUID %s\n", uuid_str.c_str());
  if(strlen(uuid_str.c_str()) != 36) {
    return false;
  }
  for(int i = 0; i < uuid_str.length(); ++i) {
    if(i % 5 == 3 && i > 5 && i < 25) {
      if (uuid_str[i] != '-') {
        return false;
      }
    } else {
      if(!isxdigit(uuid_str[i])) {
        return false;
      }
    }
  }
  return true;
}

TEST_CASE("Test C UUID generation", "[testCUUID]") {
  char uuid[37];
  CIDGenerator gen;
  for(int i = 0; i < 3; ++i) {
    gen.implementation_ = i;
    generate_uuid(&gen, uuid);
    REQUIRE(verify_uuid(uuid));
  }
}

TEST_CASE("Speed test", "[testCUUID]") {
  CIDGenerator gen;
  SECTION("CUUID_TIME_IMPL") {
    gen.implementation_ = CUUID_TIME_IMPL;
  }
  SECTION("CUUID_RANDOM_IMPL") {
    gen.implementation_ = CUUID_RANDOM_IMPL;
  }
  SECTION("CUUID_DEFAULT_IMPL") {
    gen.implementation_ = CUUID_DEFAULT_IMPL;
  }

  std::vector<std::array<char, 37U>> uuids(128U * 1024U);
  // Prime the generator
  generate_uuid(&gen, uuids[0].data());

  auto before = std::chrono::high_resolution_clock::now();
  for (size_t i = 0U; i < uuids.size(); i++) {
    generate_uuid(&gen, uuids[i].data());
  }
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - before).count();
  std::cerr << "Generating one " << gen.implementation_  << " UUID took " << (duration / uuids.size()) << "ns" << std::endl;
}

TEST_CASE("Collision test", "[testCUUID]") {
  CIDGenerator gen;
  SECTION("CUUID_TIME_IMPL") {
    gen.implementation_ = CUUID_TIME_IMPL;
  }
  SECTION("CUUID_RANDOM_IMPL") {
    gen.implementation_ = CUUID_RANDOM_IMPL;
  }
  SECTION("CUUID_DEFAULT_IMPL") {
    gen.implementation_ = CUUID_DEFAULT_IMPL;
  }

  std::vector<std::string> uuids(16 * 1024U);
  std::vector<std::thread> threads;
  for (size_t i = 0U; i < 16U; i++) {
    threads.emplace_back([&gen, &uuids, i](){
      char buffer[37];
      for (size_t j = 0U; j < 1024U; j++) {
        generate_uuid(&gen, buffer);
        uuids[i * 1024U + j] = buffer;
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }

  std::sort(uuids.begin(), uuids.end());
  REQUIRE(uuids.end() == std::adjacent_find(uuids.begin(), uuids.end()));
}
