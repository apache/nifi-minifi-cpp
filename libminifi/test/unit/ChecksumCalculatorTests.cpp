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

#include <numeric>

#include "../TestBase.h"
#include "utils/TestUtils.h"
#include "utils/ChecksumCalculator.h"

namespace {
  constexpr const char* CHECKSUM_FOR_ONE_LINE_OF_TEXT = "e26d1a9f3c3cb9f55e797ce1c9b15e89b4a23d4a301d92da23e684c8a25bf641";
  constexpr const char* CHECKSUM_FOR_TWO_LINES_OF_TEXT = "7614d0e3b10a3ae41fd50130aa8b83c1bd1248fb6dcfc25cb135665ea3c4f8c8";
}

TEST_CASE("ChecksumCalculator can calculate the checksum, which is equal to sha256sum", "[ChecksumCalculator]") {
  TestController test_controller;
  std::string test_dir = utils::createTempDir(&test_controller);
  std::string file_location = utils::putFileToDir(test_dir, "simple.txt", "one line of text\n");

  REQUIRE(std::string{utils::ChecksumCalculator::CHECKSUM_TYPE} == std::string{"SHA256"});
  REQUIRE(utils::ChecksumCalculator::LENGTH_OF_HASH_IN_BYTES == 32);

  utils::ChecksumCalculator checksum_calculator;
  checksum_calculator.setFileLocation(file_location);
  REQUIRE(checksum_calculator.getChecksum() == CHECKSUM_FOR_ONE_LINE_OF_TEXT);
}

TEST_CASE("On Windows text files, the checksum calculated is also the same as sha256sum", "[ChecksumCalculator]") {
  TestController test_controller;
  std::string test_dir = utils::createTempDir(&test_controller);
  std::string file_location = utils::putFileToDir(test_dir, "simple.txt", "one line of text\r\n");

  utils::ChecksumCalculator checksum_calculator;
  checksum_calculator.setFileLocation(file_location);
  REQUIRE(checksum_calculator.getChecksum() == "94fc46c62ef6cc5b45cbad9fd53116cfb15a80960a9b311c1c27e5b5265ad4b4");
}

TEST_CASE("The checksum can be reset and recomputed", "[ChecksumCalculator]") {
  TestController test_controller;
  std::string test_dir = utils::createTempDir(&test_controller);
  std::string file_location = utils::putFileToDir(test_dir, "simple.txt", "one line of text\n");

  utils::ChecksumCalculator checksum_calculator;
  checksum_calculator.setFileLocation(file_location);
  REQUIRE(checksum_calculator.getChecksum() == CHECKSUM_FOR_ONE_LINE_OF_TEXT);

  std::ofstream append_to_file(file_location, std::ios::binary | std::ios::app);
  append_to_file << "another line of text\n";
  append_to_file.close();

  REQUIRE(checksum_calculator.getChecksum() == CHECKSUM_FOR_ONE_LINE_OF_TEXT);  // not updated, needs to be notified

  checksum_calculator.invalidateChecksum();
  REQUIRE(checksum_calculator.getChecksum() == CHECKSUM_FOR_TWO_LINES_OF_TEXT);  // now it is updated
}

TEST_CASE("Checksums can be computed for binary (eg. encrypted) files, too", "[ChecksumCalculator]") {
  TestController test_controller;
  std::string test_dir = utils::createTempDir(&test_controller);
  std::string binary_data(size_t{256}, '\0');
  std::iota(binary_data.begin(), binary_data.end(), 'x');
  std::string file_location = utils::putFileToDir(test_dir, "simple.txt", binary_data);

  utils::ChecksumCalculator checksum_calculator;
  checksum_calculator.setFileLocation(file_location);
  REQUIRE(checksum_calculator.getChecksum() == "bdec77160c394c067419735de757e4daa1c4679ea45e82a33fa8f706eed87709");
}
