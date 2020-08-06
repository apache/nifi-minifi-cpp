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
#include <algorithm>
#include <memory>
#include <string>

#include "io/BaseStream.h"
#include "SiteToSiteHelper.h"
#include "../TestBase.h"
#include "../unit/SiteToSiteHelper.h"

#define FMT_DEFAULT fmt_lower

TEST_CASE("TestWriteUTF", "[MINIFI193]") {
  org::apache::nifi::minifi::io::BufferStream baseStream;

  std::string stringOne = "helo world";  // yes, this has a typo.
  std::string verifyString;
  baseStream.write(stringOne, false);

  baseStream.read(verifyString, false);

  REQUIRE(verifyString == stringOne);
}

TEST_CASE("TestWriteUTF2", "[MINIFI193]") {
  org::apache::nifi::minifi::io::BufferStream baseStream;

  std::string stringOne = "hel\xa1o world";
  REQUIRE(11 == stringOne.length());
  std::string verifyString;
  baseStream.write(stringOne, false);

  baseStream.read(verifyString, false);

  REQUIRE(verifyString == stringOne);
}

TEST_CASE("TestWriteUTF3", "[MINIFI193]") {
  org::apache::nifi::minifi::io::BufferStream baseStream;

  std::string stringOne = "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c";
  REQUIRE(12 == stringOne.length());
  std::string verifyString;
  baseStream.write(stringOne, false);

  baseStream.read(verifyString, false);

  REQUIRE(verifyString == stringOne);
}

TEST_CASE("Serialization test: the byteSwap functions work correctly", "[byteSwap]") {
    REQUIRE(byteSwap(uint16_t{0}) == uint16_t{0});
    REQUIRE(byteSwap(uint16_t{0x0001}) == uint16_t{0x0100});
    REQUIRE(byteSwap(uint16_t{0x0102}) == uint16_t{0x0201});
    REQUIRE(byteSwap(uint16_t{0xFFEE}) == uint16_t{0xEEFF});

    REQUIRE(byteSwap(uint32_t{0}) == uint32_t{0});
    REQUIRE(byteSwap(uint32_t{0x00000001}) == uint32_t{0x01000000});
    REQUIRE(byteSwap(uint32_t{0x01020304}) == uint32_t{0x04030201});
    REQUIRE(byteSwap(uint32_t{0xFFEEDDCC}) == uint32_t{0xCCDDEEFF});

    REQUIRE(byteSwap(uint64_t{0}) == uint64_t{0});
    REQUIRE(byteSwap(uint64_t{0x0000000000000001}) == uint64_t{0x0100000000000000});
    REQUIRE(byteSwap(uint64_t{0x0102030405060708}) == uint64_t{0x0807060504030201});
    REQUIRE(byteSwap(uint64_t{0xFFEEDDCCBBAA9988}) == uint64_t{0x8899AABBCCDDEEFF});
}
