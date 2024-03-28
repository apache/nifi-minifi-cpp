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
  TestController controller;
  // copy config file to temporary location as it will get overridden
  auto home_path = controller.createTempDirectory();
  auto live_config_file = home_path / "config.yml";
  minifi::utils::file::copy_file(test_file_path, live_config_file);
  // the C2 server will update the flow with the contents of test_file_path
  // which will be encrypted and persisted to the temporary live_config_file
  C2UpdateHandler handler(test_file_path.string());
  VerifyC2Update harness(10s);
  harness.getConfiguration()->set(minifi::Configure::nifi_flow_configuration_encrypt, "true");
  harness.setKeyDir(TEST_RESOURCES);
  harness.setUrl("https://localhost:0/update", &handler);
  handler.setC2RestResponse(harness.getC2RestUrl(), "configuration", "true");

  harness.run(live_config_file, TEST_RESOURCES);

  auto encryptor = minifi::utils::crypto::EncryptionProvider::create(TEST_RESOURCES);
  REQUIRE(encryptor);

  std::ifstream encrypted_file{live_config_file};
  std::string decrypted_config = encryptor->decrypt(std::string(std::istreambuf_iterator<char>(encrypted_file), {}));

  std::ifstream expected_file{test_file_path.string() + ".reformatted"};
  std::string expected_config{std::istreambuf_iterator<char>(expected_file), {}};

  REQUIRE(decrypted_config == expected_config);
}

}  // namespace org::apache::nifi::minifi::test
