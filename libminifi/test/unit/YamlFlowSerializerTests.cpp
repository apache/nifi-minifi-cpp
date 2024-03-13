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

#include <regex>

#include "../Catch.h"
#include "../ConfigurationTestController.h"
#include "catch2/generators/catch_generators.hpp"
#include "core/flow/FlowSchema.h"
#include "core/yaml/YamlFlowSerializer.h"
#include "core/yaml/YamlNode.h"
#include "utils/crypto/EncryptionProvider.h"
#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"
#include "utils/StringUtils.h"

namespace core = org::apache::nifi::minifi::core;
namespace utils = org::apache::nifi::minifi::utils;

constexpr std::string_view config_yaml = R"(MiNiFi Config Version: 3
Flow Controller:
  name: root
  comment: ""
Core Properties:
  flow controller graceful shutdown period: 10 sec
  flow service write delay interval: 500 ms
  administrative yield duration: 30 sec
  bored yield duration: 10 millis
  max concurrent threads: 1
  variable registry properties: ""
FlowFile Repository:
  implementation: org.apache.nifi.controller.repository.WriteAheadFlowFileRepository
  partitions: 256
  checkpoint interval: 2 mins
  always sync: false
  Swap:
    threshold: 20000
    in period: 5 sec
    in threads: 1
    out period: 5 sec
    out threads: 4
Content Repository:
  implementation: org.apache.nifi.controller.repository.FileSystemRepository
  content claim max appendable size: 10 MB
  content claim max flow files: 100
  content repository archive enabled: false
  content repository archive max retention period: 12 hours
  content repository archive max usage percentage: 50%
  always sync: false
Provenance Repository:
  provenance rollover time: 1 min
  implementation: org.apache.nifi.provenance.WriteAheadProvenanceRepository
  provenance index shard size: 500 MB
  provenance max storage size: 1 GB
  provenance max storage time: 24 hours
  provenance buffer size: 10000
Component Status Repository:
  buffer size: 1440
  snapshot frequency: 1 min
Security Properties:
  keystore: ""
  keystore type: ""
  keystore password: ""
  key password: ""
  truststore: ""
  truststore type: ""
  truststore password: ""
  ssl protocol: ""
  Sensitive Props:
    key: ~
    algorithm: NIFI_PBKDF2_AES_GCM_256
Processors:
  - id: aabb6d26-8a8d-4338-92c9-1b8c67ec18e0
    name: GenerateFlowFile
    class: org.apache.nifi.minifi.processors.GenerateFlowFile
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 15 sec
    penalization period: 30000 ms
    yield period: 1000 ms
    run duration nanos: 0
    auto-terminated relationships list: []
    Properties:
      Batch Size: 1
      Data Format: Text
      File Size: 100B
      Unique FlowFiles: true
  - id: 469617f1-3898-4bbf-91fe-27d8f4dd2a75
    name: InvokeHTTP
    class: org.apache.nifi.minifi.processors.InvokeHTTP
    max concurrent tasks: 1
    scheduling strategy: EVENT_DRIVEN
    scheduling period: 1000 ms
    penalization period: 30000 ms
    yield period: 1000 ms
    run duration nanos: 0
    auto-terminated relationships list:
      - success
      - response
      - failure
      - retry
      - no retry
    Properties:
      Always Output Response: false
      Connection Timeout: 5 s
      Content-type: application/octet-stream
      Disable Peer Verification: false
      Follow Redirects: true
      HTTP Method: POST
      Include Date Header: true
      Invalid HTTP Header Field Handling Strategy: transform
      Penalize on "No Retry": false
      Proxy Host: https://my-proxy.com
      Proxy Port: 12345
      Read Timeout: 15 s
      Remote URL: https://my-storage-server.com/postdata
      SSL Context Service: b9801278-7b5d-4314-aed6-713fd4b5f933
      Send Message Body: true
      Use Chunked Encoding: false
      invokehttp-proxy-password: very_secure_password
      invokehttp-proxy-username: user
      send-message-body: true
Controller Services:
  - id: b9801278-7b5d-4314-aed6-713fd4b5f933
    name: SSLContextService
    type: org.apache.nifi.minifi.controllers.SSLContextService
    Properties:
      CA Certificate: certs/ca-cert.pem
      Client Certificate: certs/agent-cert.pem
      Passphrase: very_secure_passphrase
      Private Key: certs/agent-key.pem
      Use System Cert Store: false
  - id: b418f4ff-e598-4ea2-921f-14f9dd864482
    name: Second SSLContextService
    type: org.apache.nifi.minifi.controllers.SSLContextService
    Properties:
      CA Certificate: second_ssl_service_certs/ca-cert.pem
      Client Certificate: second_ssl_service_certs/agent-cert.pem
      Private Key: second_ssl_service_certs/agent-key.pem
      Use System Cert Store: false
Process Groups: []
Input Ports: []
Output Ports: []
Funnels: []
Connections:
  - id: 5c3d809b-0866-4c19-8287-439e5282c9c6
    name: GenerateFlowFile/success/InvokeHTTP
    source id: aabb6d26-8a8d-4338-92c9-1b8c67ec18e0
    source relationship names:
      - success
    destination id: 469617f1-3898-4bbf-91fe-27d8f4dd2a75
    max work queue size: 2000
    max work queue data size: 100 MB
    flowfile expiration: 0 seconds
    queue prioritizer class: ""
Remote Process Groups: []
NiFi Properties Overrides: {}
)";

const utils::crypto::Bytes secret_key = utils::string::from_hex("cb76fe6fe4cbfdc3770c0cb0afc910f81ced4d436b11f691395fc2a9dbea27ca");
const utils::crypto::EncryptionProvider encryption_provider{secret_key};

TEST_CASE("YamlFlowSerializer can encrypt the sensitive properties") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_configuration{test_controller.getContext()};
  const auto process_group = yaml_configuration.getRootFromPayload(std::string{config_yaml});
  REQUIRE(process_group);

  const auto schema = core::flow::FlowSchema::getDefault();

  YAML::Node root_yaml_node = YAML::Load(std::string{config_yaml});
  const auto flow_serializer = core::yaml::YamlFlowSerializer{root_yaml_node};

  const auto processor_id = utils::Identifier::parse("469617f1-3898-4bbf-91fe-27d8f4dd2a75").value();
  const auto controller_service_id = utils::Identifier::parse("b9801278-7b5d-4314-aed6-713fd4b5f933").value();

  const auto [overrides, expected_results] = GENERATE_REF(
      std::make_tuple(core::flow::Overrides{},
          std::array{"very_secure_password", "very_secure_passphrase"}),
      std::make_tuple(core::flow::Overrides{}.add(processor_id, "invokehttp-proxy-password", "password123"),
          std::array{"password123", "very_secure_passphrase"}),
      std::make_tuple(core::flow::Overrides{}.add(controller_service_id, "Passphrase", "speak friend and enter"),
          std::array{"very_secure_password", "speak friend and enter"}),
      std::make_tuple(core::flow::Overrides{}.add(processor_id, "invokehttp-proxy-password", "password123")
                                             .add(controller_service_id, "Passphrase", "speak friend and enter"),
          std::array{"password123", "speak friend and enter"}));

  std::string config_yaml_encrypted = flow_serializer.serialize(*process_group, schema, encryption_provider, overrides);

  {
    std::regex regex{R"_(invokehttp-proxy-password: (.*))_"};
    std::smatch match_results;
    CHECK(std::regex_search(config_yaml_encrypted, match_results, regex));

    REQUIRE(match_results.size() == 2);
    std::string encrypted_value = match_results[1];
    CHECK(utils::crypto::property_encryption::decrypt(encrypted_value, encryption_provider) == expected_results[0]);
  }

  {
    std::regex regex{R"_(Passphrase: (.*))_"};
    std::smatch match_results;
    CHECK(std::regex_search(config_yaml_encrypted, match_results, regex));

    REQUIRE(match_results.size() == 2);
    std::string encrypted_value = match_results[1];
    CHECK(utils::crypto::property_encryption::decrypt(encrypted_value, encryption_provider) == expected_results[1]);
  }
}

TEST_CASE("YamlFlowSerializer with an override can add a new property to the flow config file") {
  ConfigurationTestController test_controller;
  core::YamlConfiguration yaml_configuration{test_controller.getContext()};
  const auto process_group = yaml_configuration.getRootFromPayload(std::string{config_yaml});
  REQUIRE(process_group);

  const auto schema = core::flow::FlowSchema::getDefault();

  YAML::Node root_yaml_node = YAML::Load(std::string{config_yaml});
  const auto flow_serializer = core::yaml::YamlFlowSerializer{root_yaml_node};

  const auto second_controller_service_id = utils::Identifier::parse("b418f4ff-e598-4ea2-921f-14f9dd864482").value();

  SECTION("with required overrides") {
    const auto overrides = core::flow::Overrides{}.add(second_controller_service_id, "Passphrase", "new passphrase");

    std::string config_yaml_encrypted = flow_serializer.serialize(*process_group, schema, encryption_provider, overrides);

    std::regex regex{R"_(Passphrase: (.*))_"};
    std::smatch match_results;

    // skip the first match
    REQUIRE(std::regex_search(config_yaml_encrypted.cbegin(), config_yaml_encrypted.cend(), match_results, regex));

    // verify the second match
    REQUIRE(std::regex_search(match_results.suffix().first, config_yaml_encrypted.cend(), match_results, regex));
    REQUIRE(match_results.size() == 2);
    std::string encrypted_value = match_results[1];
    CHECK(utils::crypto::property_encryption::decrypt(encrypted_value, encryption_provider) == "new passphrase");
  }

  SECTION("with optional overrides: the override is only used if the property is already in the flow config") {
    const auto first_controller_service_id = utils::Identifier::parse("b9801278-7b5d-4314-aed6-713fd4b5f933").value();
    const auto overrides = core::flow::Overrides{}
        .addOptional(first_controller_service_id, "Passphrase", "first new passphrase")
        .addOptional(second_controller_service_id, "Passphrase", "second new passphrase");

    std::string config_yaml_encrypted = flow_serializer.serialize(*process_group, schema, encryption_provider, overrides);

    std::regex regex{R"_(Passphrase: (.*))_"};
    std::smatch match_results;

    // verify the first match
    REQUIRE(std::regex_search(config_yaml_encrypted.cbegin(), config_yaml_encrypted.cend(), match_results, regex));
    REQUIRE(match_results.size() == 2);
    std::string encrypted_value = match_results[1];
    CHECK(utils::crypto::property_encryption::decrypt(encrypted_value, encryption_provider) == "first new passphrase");

    // check that there is no second match
    CHECK_FALSE(std::regex_search(match_results.suffix().first, config_yaml_encrypted.cend(), match_results, regex));
  }
}
