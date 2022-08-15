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
#include <string>
#include "Catch.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "properties/Configuration.h"
#include "ConfigTestAccessor.h"

// from TestHTTPGet.yml
constexpr auto invokehttp_uuid = "2438e3c8-015a-1000-79ca-83af40ec1991";
constexpr auto logattribute_uuid = "2438e3c8-015a-1000-79ca-83af40ec1992";

class DescribeManifestHandler: public HeartbeatHandler {
 public:
  explicit DescribeManifestHandler(std::shared_ptr<minifi::Configure> configuration, std::atomic<bool>& verified)
    : HeartbeatHandler(std::move(configuration)),
      verified_(verified) {
  }

  void handleHeartbeat(const rapidjson::Document&, struct mg_connection * conn) override {
    sendHeartbeatResponse("DESCRIBE", "manifest", "889345", conn);
  }

  void handleAcknowledge(const rapidjson::Document& root) override {
    verifyJsonHasAgentManifest(root, {invokehttp_uuid, logattribute_uuid}, {"nifi.extension.path", "nifi.python.processor.dir"});
    verified_ = true;
  }

 private:
  std::atomic<bool>& verified_;
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv, "heartbeat");
  std::atomic_bool verified{false};
  VerifyC2Describe harness(verified);
  utils::crypto::Bytes encryption_key = utils::StringUtils::from_hex("4024b327fdc987ce3eb43dd1f690b9987e4072e0020e3edf4349ce1ad91a4e38");
  minifi::Decryptor decryptor{utils::crypto::EncryptionProvider{encryption_key}};
  std::string encrypted_value = "l3WY1V27knTiPa6jVX0jrq4qjmKsySOu||ErntqZpHP1M+6OkA14p5sdnqJhuNHWHDVUU5EyMloTtSytKk9a5xNKo=";

  harness.setConfiguration(std::make_shared<minifi::Configure>(decryptor));
  harness.setKeyDir(args.key_dir);
  DescribeManifestHandler responder(harness.getConfiguration(), verified);

  auto logger_properties = std::make_shared<core::logging::LoggerProperties>();
  ConfigTestAccessor::call_setLoggerProperties(harness.getConfiguration(), logger_properties);

  harness.getConfiguration()->set(minifi::Configuration::nifi_rest_api_password, encrypted_value);
  harness.getConfiguration()->set(std::string(minifi::Configuration::nifi_rest_api_password) + ".protected", utils::crypto::EncryptionType::name());
  harness.getConfiguration()->set(minifi::Configuration::nifi_server_name, "server_name");
  harness.getConfiguration()->set(minifi::Configuration::nifi_framework_dir, "framework_path");
  harness.getConfiguration()->set(minifi::Configuration::nifi_sensitive_props_additional_keys,
    std::string(minifi::Configuration::nifi_framework_dir) + ", " + std::string(minifi::Configuration::nifi_server_name));
  harness.getConfiguration()->set(minifi::Configuration::nifi_log_appender_rolling_directory, "/var/log/minifi");

  harness.setUrl(args.url, &responder);
  harness.run(args.test_file);
}
