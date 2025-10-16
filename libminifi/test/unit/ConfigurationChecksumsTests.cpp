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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"
#include "core/state/nodes/ConfigurationChecksums.h"
#include "utils/ChecksumCalculator.h"

using org::apache::nifi::minifi::state::response::ConfigurationChecksums;

TEST_CASE("If no checksum calculators are added, we get an empty node", "[ConfigurationChecksums]") {
  ConfigurationChecksums configuration_checksums;

  const auto serialized_response_nodes = configuration_checksums.serialize();

  REQUIRE(serialized_response_nodes.size() == 1);
  const auto& checksum_node = serialized_response_nodes[0];
  REQUIRE(checksum_node.children.empty());
}

TEST_CASE("If one checksum calculator is added, we get a node with one child", "[ConfigurationChecksums]") {
  TestController test_controller;
  auto test_dir = test_controller.createTempDirectory();
  auto file_location = minifi::test::utils::putFileToDir(test_dir, "simple.txt", "one line of text\n");

  utils::ChecksumCalculator checksum_calculator;
  checksum_calculator.setFileLocations(std::vector{file_location});

  ConfigurationChecksums configuration_checksums;
  configuration_checksums.addChecksumCalculator(checksum_calculator);

  const auto serialized_response_nodes = configuration_checksums.serialize();

  REQUIRE(serialized_response_nodes.size() == 1);
  const auto& checksum_node = serialized_response_nodes[0];
  REQUIRE(checksum_node.children.size() == 1);

  const auto& file_checksum_node = checksum_node.children[0];
  REQUIRE(file_checksum_node.name == "simple.txt");
  REQUIRE(file_checksum_node.value == "e26d1a9f3c3cb9f55e797ce1c9b15e89b4a23d4a301d92da23e684c8a25bf641");
}

TEST_CASE("If two checksum calculators are added, we get a node with two children", "[ConfigurationChecksums]") {
  TestController test_controller;
  auto test_dir = test_controller.createTempDirectory();
  auto file_location_1 = minifi::test::utils::putFileToDir(test_dir, "first.txt", "this is the first file\n");
  auto file_location_2 = minifi::test::utils::putFileToDir(test_dir, "second.txt", "this is the second file\n");

  utils::ChecksumCalculator checksum_calculator_1;
  checksum_calculator_1.setFileLocations(std::vector{file_location_1});
  utils::ChecksumCalculator checksum_calculator_2;
  checksum_calculator_2.setFileLocations(std::vector{file_location_2});

  ConfigurationChecksums configuration_checksums;
  configuration_checksums.addChecksumCalculator(checksum_calculator_1);
  configuration_checksums.addChecksumCalculator(checksum_calculator_2);

  const auto serialized_response_nodes = configuration_checksums.serialize();

  REQUIRE(serialized_response_nodes.size() == 1);
  const auto& checksum_node = serialized_response_nodes[0];
  REQUIRE(checksum_node.children.size() == 2);

  const auto& file_checksum_node_1 = checksum_node.children[0];
  REQUIRE(file_checksum_node_1.name == "first.txt");
  REQUIRE(file_checksum_node_1.value == "413a91c71ae3cc76e641a7fdfe8a18ab043f9eb9838edcaadfb71194385653c2");

  const auto& file_checksum_node_2 = checksum_node.children[1];
  REQUIRE(file_checksum_node_2.name == "second.txt");
  REQUIRE(file_checksum_node_2.value == "d2931f19097c7a8382567a45f617d567c26d10b547c7243f04e77f83af620d42");
}
