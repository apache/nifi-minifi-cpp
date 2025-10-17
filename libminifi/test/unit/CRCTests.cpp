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
#include <vector>
#include "io/CRCStream.h"
#include "minifi-cpp/io/OutputStream.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "minifi-cpp/utils/gsl.h"

TEST_CASE("Test CRC1", "[testcrc1]") {
  org::apache::nifi::minifi::io::BufferStream base;
  org::apache::nifi::minifi::io::CRCStream<org::apache::nifi::minifi::io::OutputStream> test(gsl::make_not_null(&base));
  test.write(reinterpret_cast<const uint8_t*>("cow"), 3);
  REQUIRE(2580823964 == test.getCRC());
}

TEST_CASE("Test CRC2", "[testcrc2]") {
  org::apache::nifi::minifi::io::BufferStream base;
  org::apache::nifi::minifi::io::CRCStream<org::apache::nifi::minifi::io::OutputStream> test(gsl::make_not_null(&base));
  std::string fox = "the quick brown fox jumped over the brown fox";
  std::vector<uint8_t> charvect(fox.begin(), fox.end());
  test.write(charvect, charvect.size());
  REQUIRE(1922388889 == test.getCRC());
}

TEST_CASE("Test CRC3", "[testcrc3]") {
  org::apache::nifi::minifi::io::BufferStream base;
  org::apache::nifi::minifi::io::CRCStream<org::apache::nifi::minifi::io::OutputStream> test(gsl::make_not_null(&base));
  uint64_t number = 7;
  test.write(number);
  REQUIRE(4215687882 == test.getCRC());
}

TEST_CASE("Test CRC4", "[testcrc4]") {
  org::apache::nifi::minifi::io::BufferStream base;
  org::apache::nifi::minifi::io::CRCStream<org::apache::nifi::minifi::io::OutputStream> test(gsl::make_not_null(&base));
  uint32_t number = 7;
  test.write(number);
  REQUIRE(3206564543 == test.getCRC());
}

TEST_CASE("Test CRC5", "[testcrc5]") {
  org::apache::nifi::minifi::io::BufferStream base;
  org::apache::nifi::minifi::io::CRCStream<org::apache::nifi::minifi::io::OutputStream> test(gsl::make_not_null(&base));
  uint16_t number = 7;
  test.write(number);
  REQUIRE(3753740124 == test.getCRC());
}

TEST_CASE("CRCStream with initial crc = 0 is the same as without initial crc", "[initial_crc_arg]") {
  org::apache::nifi::minifi::io::BufferStream base1;
  org::apache::nifi::minifi::io::CRCStream<org::apache::nifi::minifi::io::OutputStream> test_noinit(gsl::make_not_null(&base1));

  org::apache::nifi::minifi::io::BufferStream base2;
  org::apache::nifi::minifi::io::CRCStream<org::apache::nifi::minifi::io::OutputStream> test_initzero(gsl::make_not_null(&base2), 0);

  const std::string textString = "The quick brown fox jumps over the lazy dog";
  std::vector<uint8_t> textVector1(textString.begin(), textString.end());
  std::vector<uint8_t> textVector2(textString.begin(), textString.end());

  test_noinit.write(textVector1, textVector1.size());
  test_initzero.write(textVector2, textVector2.size());
  REQUIRE(test_noinit.getCRC() == test_initzero.getCRC());
}

TEST_CASE("CRCStream: one long write is the same as writing in two pieces", "[initial_crc_arg]") {
  const std::string textString = "The quick brown fox jumps over the lazy dog";

  org::apache::nifi::minifi::io::BufferStream base_full;
  org::apache::nifi::minifi::io::CRCStream<org::apache::nifi::minifi::io::OutputStream> test_full(gsl::make_not_null(&base_full), 0);
  std::vector<uint8_t> textVector_full(textString.begin(), textString.end());
  test_full.write(textVector_full, textVector_full.size());

  org::apache::nifi::minifi::io::BufferStream base_piece1;
  org::apache::nifi::minifi::io::CRCStream<org::apache::nifi::minifi::io::OutputStream> test_piece1(gsl::make_not_null(&base_piece1), 0);
  std::vector<uint8_t> textVector_piece1(textString.begin(), textString.begin() + 15);
  test_piece1.write(textVector_piece1, textVector_piece1.size());

  org::apache::nifi::minifi::io::BufferStream base_piece2;
  org::apache::nifi::minifi::io::CRCStream<org::apache::nifi::minifi::io::OutputStream> test_piece2(gsl::make_not_null(&base_piece2), test_piece1.getCRC());
  std::vector<uint8_t> textVector_piece2(textString.begin() + 15, textString.end());
  test_piece2.write(textVector_piece2, textVector_piece2.size());

  REQUIRE(test_full.getCRC() == test_piece2.getCRC());
}
