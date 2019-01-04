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
#include "TestBase.h"
#include "core/cuuid.h"

bool verify_uuid(const char * uuid) {
  std::string uuid_str(uuid, 36);
  if(uuid_str.length() != 36) {
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
  char uuid[36];
  CIDGenerator gen;
  for(int i = 0; i < 4; ++i) {
    generate_uuid(&gen, uuid);
    REQUIRE(verify_uuid(uuid));
    gen.implementation_ = i;
  }
}
