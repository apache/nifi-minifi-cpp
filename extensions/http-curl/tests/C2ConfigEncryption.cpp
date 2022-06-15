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

#undef NDEBUG
#include <iterator>
#include <fstream>
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "utils/IntegrationTestUtils.h"
#include "utils/crypto/EncryptionProvider.h"

using namespace std::literals::chrono_literals;

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv, "update");
  TestController controller;
  // copy config file to temporary location as it will get overridden
  std::string home_path = controller.createTempDirectory();
  std::string live_config_file = utils::file::concat_path(home_path, "config.yml");
  utils::file::copy_file(args.test_file, live_config_file);
  // the C2 server will update the flow with the contents of args.test_file
  // which will be encrypted and persisted to the temporary live_config_file
  C2UpdateHandler handler(args.test_file);
  VerifyC2Update harness(10s);
  harness.getConfiguration()->set(minifi::Configure::nifi_flow_configuration_encrypt, "true");
  harness.setKeyDir(args.key_dir);
  harness.setUrl(args.url, &handler);
  handler.setC2RestResponse(harness.getC2RestUrl(), "configuration", "true");

  harness.run(live_config_file, args.key_dir);

  auto encryptor = utils::crypto::EncryptionProvider::create(args.key_dir);
  assert(encryptor);

  std::ifstream encrypted_file{live_config_file, std::ios::binary};
  std::string decrypted_config = encryptor->decrypt(std::string(std::istreambuf_iterator<char>(encrypted_file), {}));

  std::ifstream original_file{args.test_file, std::ios::binary};
  std::string original_config{std::istreambuf_iterator<char>(original_file), {}};

  assert(decrypted_config == original_config);
}
