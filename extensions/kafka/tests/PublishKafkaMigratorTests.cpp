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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "PublishKafka.h"
#include "unit/ConfigurationTestController.h"
#include "core/flow/AdaptiveConfiguration.h"
#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"

#include "yaml-cpp/yaml.h"

namespace org::apache::nifi::minifi::test {

using minifi::utils::crypto::property_encryption::decrypt;

TEST_CASE("PublishKafkaMigratorTest yaml") {
  static constexpr std::string_view ORIGINAL_YAML = R"(
MiNiFi Config Version: 3
Flow Controller:
  name: MiNiFi Flow
Processors:
  - name: Get files from /tmp/input
    id: 7fd166aa-0662-4c42-affa-88f6fb39807f
    class: org.apache.nifi.processors.standard.GetFile
    scheduling period: 2 sec
    scheduling strategy: TIMER_DRIVEN
    Properties:
      Input Directory: /tmp/input
  - name: Publish messages to Kafka topic test
    id: 8a534b4a-2b4a-4e1e-ab07-8a09fa08f848
    class: org.apache.nifi.processors.standard.PublishKafka
    scheduling strategy: EVENT_DRIVEN
    auto-terminated relationships list:
      - success
      - failure
    Properties:
      Message Key Field: foo
      Batch Size: 10
      Client Name: test-client
      Compress Codec: none
      Delivery Guarantee: 1
      Known Brokers: kafka-broker:9092
      Message Timeout: 12 sec
      Request Timeout: 10 sec
      Topic Name: test
      Security CA: /tmp/resources/certs/ca-cert
      Security Cert: /tmp/resources/certs/client_test_client_client.pem
      Security Pass Phrase: abcdefgh
      Security Private Key: /tmp/resources/certs/client_test_client_client.key
Connections:
  - name: GetFile/success/PublishKafka
    id: 1edd529e-eee9-4b05-9e35-f1607bb0243b
    source id: 7fd166aa-0662-4c42-affa-88f6fb39807f
    source relationship name: success
    destination id: 8a534b4a-2b4a-4e1e-ab07-8a09fa08f848
Controller Services: []
Remote Processing Groups: []
)";
  ConfigurationTestController test_controller;
  std::string serialized_flow_definition;
  SECTION("YamlConfiguration") {
    core::YamlConfiguration yaml_config(test_controller.getContext());
    auto root_flow_definition = yaml_config.getRootFromPayload(std::string{ORIGINAL_YAML});
    REQUIRE(root_flow_definition);
    serialized_flow_definition = yaml_config.serialize(*root_flow_definition);
  }
  SECTION("Adaptive Yaml Configuration") {
    core::flow::AdaptiveConfiguration adaptive_configuration(test_controller.getContext());
    auto root_flow_definition = adaptive_configuration.getRootFromPayload(std::string{ORIGINAL_YAML});
    REQUIRE(root_flow_definition);
    serialized_flow_definition = adaptive_configuration.serialize(*root_flow_definition);
  }
  YAML::Node migrated_flow = YAML::Load(std::string{serialized_flow_definition});
  CHECK(migrated_flow["Controller Services"].IsSequence());
  CHECK(migrated_flow["Controller Services"].size() == 1);
  CHECK(migrated_flow["Controller Services"][0]["name"].as<std::string>() == "GeneratedSSLContextServiceFor_8a534b4a-2b4a-4e1e-ab07-8a09fa08f848");
  CHECK(migrated_flow["Controller Services"][0]["class"].as<std::string>() == "SSLContextService");
  CHECK(migrated_flow["Controller Services"][0]["Properties"].IsMap());
  CHECK(migrated_flow["Controller Services"][0]["Properties"]["CA Certificate"].as<std::string>() == "/tmp/resources/certs/ca-cert");
  CHECK(migrated_flow["Controller Services"][0]["Properties"]["Client Certificate"].as<std::string>() == "/tmp/resources/certs/client_test_client_client.pem");
  CHECK(migrated_flow["Controller Services"][0]["Properties"]["Private Key"].as<std::string>() == "/tmp/resources/certs/client_test_client_client.key");
  CHECK(decrypt(migrated_flow["Controller Services"][0]["Properties"]["Passphrase"].as<std::string>(), test_controller.sensitive_values_encryptor_) == "abcdefgh");

  CHECK_FALSE(migrated_flow["Processors"][1]["Properties"]["Message Key Field"].IsDefined());

  CHECK_FALSE(migrated_flow["Processors"][1]["Properties"]["Security CA"].IsDefined());
  CHECK_FALSE(migrated_flow["Processors"][1]["Properties"]["Security Cert"].IsDefined());
  CHECK_FALSE(migrated_flow["Processors"][1]["Properties"]["Security Pass Phrase"].IsDefined());
  CHECK_FALSE(migrated_flow["Processors"][1]["Properties"]["Security Private Key"].IsDefined());

  CHECK(migrated_flow["Processors"][1]["Properties"]["SSL Context Service"].as<std::string>() == "GeneratedSSLContextServiceFor_8a534b4a-2b4a-4e1e-ab07-8a09fa08f848");
}

TEST_CASE("PublishKafkaMigratorTest json") {
  static constexpr std::string_view ORIGINAL_JSON = R"(
{
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [
      {
        "name": "Get files from /tmp/input",
        "identifier": "7fd166aa-0662-4c42-affa-88f6fb39807f",
        "type": "org.apache.nifi.processors.standard.GetFile",
        "schedulingStrategy": "TIMER_DRIVEN",
        "schedulingPeriod": "2 sec",
        "properties": {
          "Input Directory": "/tmp/input"
        },
        "autoTerminatedRelationships": []
      },
      {
        "name": "Publish messages to Kafka topic test",
        "identifier": "8a534b4a-2b4a-4e1e-ab07-8a09fa08f848",
        "type": "org.apache.nifi.processors.standard.PublishKafka",
        "schedulingStrategy": "EVENT_DRIVEN",
        "properties": {
          "Batch Size": "10",
          "Client Name": "test-client",
          "Compress Codec": "none",
          "Delivery Guarantee": "1",
          "Known Brokers": "kafka-broker:9092",
          "Message Timeout": "12 sec",
          "Request Timeout": "10 sec",
          "Topic Name": "test",
          "Security CA": "/tmp/resources/certs/ca-cert",
          "Security Cert": "/tmp/resources/certs/client_test_client_client.pem",
          "Security Pass Phrase": "abcdefgh",
          "Security Private Key": "/tmp/resources/certs/client_test_client_client.key"
        },
        "autoTerminatedRelationships": [
          "success",
          "failure"
        ]
      }
    ],
    "connections": [
      {
        "identifier": "1edd529e-eee9-4b05-9e35-f1607bb0243b",
        "name": "GetFile/success/PublishKafka",
        "source": {
          "id": "7fd166aa-0662-4c42-affa-88f6fb39807f"
        },
        "destination": {
          "id": "8a534b4a-2b4a-4e1e-ab07-8a09fa08f848"
        },
        "selectedRelationships": [
          "success"
        ]
      }
    ],
    "remoteProcessGroups": [],
    "controllerServices": []
  }
}
)";
  ConfigurationTestController test_controller;
  core::flow::AdaptiveConfiguration adaptive_configuration(test_controller.getContext());
  auto root_flow_definition = adaptive_configuration.getRootFromPayload(std::string{ORIGINAL_JSON});
  REQUIRE(root_flow_definition);
  std::string serialized_flow_definition = adaptive_configuration.serialize(*root_flow_definition);
  rapidjson::Document migrated_flow;
  const rapidjson::ParseResult json_parse_result = migrated_flow.Parse(serialized_flow_definition.c_str(), serialized_flow_definition.length());
  REQUIRE(json_parse_result);
  CHECK(migrated_flow["rootGroup"]["controllerServices"].IsArray());
  CHECK(migrated_flow["rootGroup"]["controllerServices"].Size() == 1);
  CHECK(migrated_flow["rootGroup"]["controllerServices"][0]["name"] == "GeneratedSSLContextServiceFor_8a534b4a-2b4a-4e1e-ab07-8a09fa08f848");
  CHECK(migrated_flow["rootGroup"]["controllerServices"][0]["type"] == "SSLContextService");
  CHECK(migrated_flow["rootGroup"]["controllerServices"][0]["properties"].IsObject());
  CHECK(migrated_flow["rootGroup"]["controllerServices"][0]["properties"]["CA Certificate"] == "/tmp/resources/certs/ca-cert");
  CHECK(migrated_flow["rootGroup"]["controllerServices"][0]["properties"]["Client Certificate"] == "/tmp/resources/certs/client_test_client_client.pem");
  CHECK(migrated_flow["rootGroup"]["controllerServices"][0]["properties"]["Private Key"] == "/tmp/resources/certs/client_test_client_client.key");
  CHECK(decrypt(migrated_flow["rootGroup"]["controllerServices"][0]["properties"]["Passphrase"].GetString(), test_controller.sensitive_values_encryptor_) == "abcdefgh");

  CHECK_FALSE(migrated_flow["rootGroup"]["processors"][1]["properties"].HasMember("Message Key Field"));

  CHECK_FALSE(migrated_flow["rootGroup"]["processors"][1]["properties"].HasMember("Security CA"));
  CHECK_FALSE(migrated_flow["rootGroup"]["processors"][1]["properties"].HasMember("Security Cert"));
  CHECK_FALSE(migrated_flow["rootGroup"]["processors"][1]["properties"].HasMember("Security Pass Phrase"));
  CHECK_FALSE(migrated_flow["rootGroup"]["processors"][1]["properties"].HasMember("Security Private Key"));

  CHECK(migrated_flow["rootGroup"]["processors"][1]["properties"]["SSL Context Service"] == "GeneratedSSLContextServiceFor_8a534b4a-2b4a-4e1e-ab07-8a09fa08f848");
}


}  // namespace org::apache::nifi::minifi::test
