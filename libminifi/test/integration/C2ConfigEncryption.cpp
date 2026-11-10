// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <iterator>
#include <fstream>
#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "unit/TestUtils.h"
#include "utils/crypto/EncryptionProvider.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

TEST_CASE("C2ConfigEncryption", "[c2test]") {
  std::filesystem::path test_file_path;
  SECTION("Yaml config") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "decrypted.config.yml";
  }
  SECTION("Json config") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "decrypted.config.json";
  }

  C2UpdateHandler handler(test_file_path.string());
  VerifyC2Update harness(test_file_path, TEST_RESOURCES, 10s);
  harness.getConfiguration()->set(minifi::Configure::nifi_flow_configuration_encrypt, "true");
  harness.setKeyDir(TEST_RESOURCES);
  harness.setUrl("https://localhost:0/update", &handler);
  handler.setC2RestResponse(harness.getC2RestUrl(), "configuration", "true");

  harness.run();
  auto live_config_file = harness.getFlowConfigPath();

  auto encryptor = minifi::utils::crypto::EncryptionProvider::create(TEST_RESOURCES);
  REQUIRE(encryptor);

  std::ifstream encrypted_file{*live_config_file};
  std::string decrypted_config = encryptor->decrypt(std::string(std::istreambuf_iterator<char>(encrypted_file), {}));

  std::ifstream expected_file{test_file_path.string() + ".reformatted"};
  std::string expected_config{std::istreambuf_iterator<char>(expected_file), {}};

  REQUIRE(decrypted_config == expected_config);
}

}  // namespace org::apache::nifi::minifi::test
