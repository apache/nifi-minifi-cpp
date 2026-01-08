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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"
#include "utils/ChecksumCalculator.h"

namespace {
  constexpr const char* CHECKSUM_FOR_ONE_LINE_OF_TEXT = "e26d1a9f3c3cb9f55e797ce1c9b15e89b4a23d4a301d92da23e684c8a25bf641";
  constexpr const char* CHECKSUM_FOR_TWO_LINES_OF_TEXT = "7614d0e3b10a3ae41fd50130aa8b83c1bd1248fb6dcfc25cb135665ea3c4f8c8";
}

TEST_CASE("ChecksumCalculator can calculate the checksum, which is equal to sha256sum", "[ChecksumCalculator]") {
  TestController test_controller;
  auto test_dir = test_controller.createTempDirectory();
  auto file_location = minifi::test::utils::putFileToDir(test_dir, "simple.txt", "one line of text\n");

  REQUIRE(std::string{utils::ChecksumCalculator::CHECKSUM_TYPE} == std::string{"SHA256"});
  // the first size_t{} is required by Catch2; it can't use a constexpr expression directly
  REQUIRE(size_t{utils::ChecksumCalculator::LENGTH_OF_HASH_IN_BYTES} == size_t{32});

  utils::ChecksumCalculator checksum_calculator;
  checksum_calculator.setFileLocations(std::vector{file_location});
  REQUIRE(checksum_calculator.getChecksum() == CHECKSUM_FOR_ONE_LINE_OF_TEXT);
}

TEST_CASE("The input of ChecksumCalculator can be in multiple files", "[ChecksumCalculator]") {
  TestController test_controller;
  const auto test_dir = test_controller.createTempDirectory();
  const auto single_file_location = minifi::test::utils::putFileToDir(test_dir, "single.txt", "one line of text\nsecond line of text\n");
  const auto multiple_1_file_location = minifi::test::utils::putFileToDir(test_dir, "multiple_1.txt", "one line of text\n");
  const auto multiple_2_file_location = minifi::test::utils::putFileToDir(test_dir, "multiple_2.txt", "second line of text\n");

  utils::ChecksumCalculator checksum_calculator_single;
  checksum_calculator_single.setFileLocations(std::vector{single_file_location});

  utils::ChecksumCalculator checksum_calculator_multiple;
  checksum_calculator_multiple.setFileLocations(std::vector{multiple_1_file_location, multiple_2_file_location});

  CHECK(checksum_calculator_single.getChecksum() == checksum_calculator_multiple.getChecksum());
}

TEST_CASE("On Windows text files, the checksum calculated is also the same as sha256sum", "[ChecksumCalculator]") {
  TestController test_controller;
  auto test_dir = test_controller.createTempDirectory();
  auto file_location = minifi::test::utils::putFileToDir(test_dir, "simple.txt", "one line of text\r\n");

  utils::ChecksumCalculator checksum_calculator;
  checksum_calculator.setFileLocations(std::vector{file_location});
  REQUIRE(checksum_calculator.getChecksum() == "94fc46c62ef6cc5b45cbad9fd53116cfb15a80960a9b311c1c27e5b5265ad4b4");
}

TEST_CASE("The checksum can be reset and recomputed", "[ChecksumCalculator]") {
  TestController test_controller;
  auto test_dir = test_controller.createTempDirectory();
  auto file_location = minifi::test::utils::putFileToDir(test_dir, "simple.txt", "one line of text\n");

  utils::ChecksumCalculator checksum_calculator;
  checksum_calculator.setFileLocations(std::vector{file_location});
  REQUIRE(checksum_calculator.getChecksum() == CHECKSUM_FOR_ONE_LINE_OF_TEXT);

  std::ofstream append_to_file(file_location, std::ios::binary | std::ios::app);
  append_to_file << "another line of text\n";
  append_to_file.close();

  REQUIRE(checksum_calculator.getChecksum() == CHECKSUM_FOR_ONE_LINE_OF_TEXT);  // not updated, needs to be notified

  checksum_calculator.invalidateChecksum();
  REQUIRE(checksum_calculator.getChecksum() == CHECKSUM_FOR_TWO_LINES_OF_TEXT);  // now it is updated
}

TEST_CASE("If the file location is updated, the checksum will be recomputed", "[ChecksumCalculator]") {
  TestController test_controller;
  auto test_dir = test_controller.createTempDirectory();
  auto file_location = minifi::test::utils::putFileToDir(test_dir, "simple.txt", "one line of text\n");

  utils::ChecksumCalculator checksum_calculator;
  checksum_calculator.setFileLocations(std::vector{file_location});
  REQUIRE(checksum_calculator.getChecksum() == CHECKSUM_FOR_ONE_LINE_OF_TEXT);

  auto other_file_location = minifi::test::utils::putFileToDir(test_dir, "long.txt", "one line of text\nanother line of text\n");
  checksum_calculator.setFileLocations(std::vector{other_file_location});
  REQUIRE(checksum_calculator.getChecksum() == CHECKSUM_FOR_TWO_LINES_OF_TEXT);
}

TEST_CASE("Checksums can be computed for binary (eg. encrypted) files, too", "[ChecksumCalculator]") {
  TestController test_controller;
  auto test_dir = test_controller.createTempDirectory();
  std::string binary_data(size_t{256}, '\0');
  std::iota(binary_data.begin(), binary_data.end(), 'x');
  auto file_location = minifi::test::utils::putFileToDir(test_dir, "simple.txt", binary_data);

  utils::ChecksumCalculator checksum_calculator;
  checksum_calculator.setFileLocations(std::vector{file_location});
  REQUIRE(checksum_calculator.getChecksum() == "bdec77160c394c067419735de757e4daa1c4679ea45e82a33fa8f706eed87709");
}

TEST_CASE("The agent identifier is excluded from the checksum", "[ChecksumCalculator]") {
  TestController test_controller;
  auto test_dir = test_controller.createTempDirectory();
  auto file_location_1 = minifi::test::utils::putFileToDir(test_dir, "agent_one.txt",
      "nifi.c2.agent.class=Test\n"
      "nifi.c2.agent.identifier=Test-111\n"
      "nifi.c2.agent.heartbeat.period=10 sec\n");
  auto file_location_2 = minifi::test::utils::putFileToDir(test_dir, "agent_two.txt",
      "nifi.c2.agent.class=Test\n"
      "nifi.c2.agent.identifier=Test-222\n"
      "nifi.c2.agent.heartbeat.period=10 sec\n");

  utils::ChecksumCalculator checksum_calculator_1;
  checksum_calculator_1.setFileLocations(std::vector{file_location_1});
  utils::ChecksumCalculator checksum_calculator_2;
  checksum_calculator_2.setFileLocations(std::vector{file_location_2});

  REQUIRE(checksum_calculator_1.getChecksum() == checksum_calculator_2.getChecksum());
}

TEST_CASE("ChecksumCalculator::getChecksum will throw if the file does not exist", "[ChecksumCalculator]") {
  utils::ChecksumCalculator checksum_calculator;
  checksum_calculator.setFileLocations(std::vector{std::filesystem::path{"/this/file/does/not/exist/84a77fd9-16b3-49d2-aead-a1f9e58e530d"}});

  REQUIRE_THROWS(checksum_calculator.getChecksum());
}
