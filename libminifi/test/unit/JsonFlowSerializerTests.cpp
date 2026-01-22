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

#include "unit/Catch.h"
#include "unit/ConfigurationTestController.h"
#include "catch2/generators/catch_generators.hpp"
#include "core/flow/FlowSchema.h"
#include "core/json/JsonFlowSerializer.h"
#include "core/json/JsonNode.h"
#include "utils/crypto/EncryptionProvider.h"
#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"
#include "utils/StringUtils.h"
#include "core/Resource.h"
#include "utils/Environment.h"

namespace org::apache::nifi::minifi::test {

constexpr std::string_view config_json_with_default_schema = R"({
  "MiNiFi Config Version": 3,
  "Flow Controller": {
    "name": "MiNiFi Flow"
  },
  "Parameter Contexts": [
    {
        "id": "721e10b7-8e00-3188-9a27-476cca376978",
        "name": "my-context",
        "description": "my parameter context",
        "Parameters": [
            {
                "name": "secret_parameter",
                "description": "",
                "sensitive": true,
                "value": "param_value_1"
            }
        ]
    }
],
  "Processors": [
    {
      "name": "Generate random data",
      "id": "aabb6d26-8a8d-4338-92c9-1b8c67ec18e0",
      "class": "org.apache.nifi.minifi.processors.GenerateFlowFile",
      "scheduling strategy": "TIMER_DRIVEN",
      "scheduling period": "15 sec",
      "Properties": {
        "File Size": "100B",
        "Batch Size": "1",
        "Unique FlowFiles": "true",
        "Data Format": "Text"
      }
    },
    {
      "name": "Post files via secure HTTP",
      "id": "469617f1-3898-4bbf-91fe-27d8f4dd2a75",
      "class": "org.apache.nifi.minifi.processors.InvokeHTTP",
      "scheduling strategy": "EVENT_DRIVEN",
      "auto-terminated relationships list": ["success", "response", "failure", "retry", "no retry"],
      "Properties": {
        "Proxy Host": "https://my-proxy.com",
        "Invalid HTTP Header Field Handling Strategy": "transform",
        "Read Timeout": "15 s",
        "invokehttp-proxy-password": "very_secure_password",
        "Send Message Body": "true",
        "Proxy Port": "12345",
        "invokehttp-proxy-username": "user",
        "Connection Timeout": "5 s",
        "send-message-body": "true",
        "Content-type": "application/octet-stream",
        "SSL Context Service": "b9801278-7b5d-4314-aed6-713fd4b5f933",
        "Always Output Response": "false",
        "HTTP Method": "POST",
        "Include Date Header": "true",
        "Use Chunked Encoding": "false",
        "Disable Peer Verification": "false",
        "Penalize on \"No Retry\"": "false",
        "Follow Redirects": "true",
        "Remote URL": "https://my-storage-server.com/postdata"
      }
    }
  ],
  "Connections": [
    {
      "name": "GenerateFlowFile/success/InvokeHTTP",
      "id": "5c3d809b-0866-4c19-8287-439e5282c9c6",
      "source id": "aabb6d26-8a8d-4338-92c9-1b8c67ec18e0",
      "source relationship names": ["success"],
      "destination id": "469617f1-3898-4bbf-91fe-27d8f4dd2a75"
    }
  ],
  "Controller Services": [
    {
      "name": "SSLContextService",
      "id": "b9801278-7b5d-4314-aed6-713fd4b5f933",
      "class": "org.apache.nifi.minifi.controllers.SSLContextService",
      "Properties": {
        "Private Key": "certs/agent-key.pem",
        "Client Certificate": "certs/agent-cert.pem",
        "Passphrase": "very_secure_passphrase",
        "CA Certificate": "certs/ca-cert.pem",
        "Use System Cert Store": "false"
      }
    }
  ],
  "Remote Process Groups": []
}
)";

// in two parts because Visual Studio doesn't like long string constants
constexpr std::string_view config_json_with_nifi_schema_part_1 = R"({
    "encodingVersion": {
        "majorVersion": 2,
        "minorVersion": 0
    },
    "maxTimerDrivenThreadCount": 1,
    "maxEventDrivenThreadCount": 1,
    "parameterContexts": [
        {
            "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
            "name": "my-context",
            "description": "my parameter context",
            "parameters": [
                {
                    "name": "secret_parameter",
                    "description": "",
                    "sensitive": true,
                    "value": "param_value_1"
                }
            ]
        }
    ],
    "rootGroup": {
        "identifier": "8b4d66dc-9085-4722-b35b-3492f363baa3",
        "instanceIdentifier": "0fab8422-3fbe-45e0-bd3a-324f5cb0592b",
        "name": "root",
        "position": {
            "x": 0.0,
            "y": 0.0
        },
        "processGroups": [],
        "remoteProcessGroups": [],
        "processors": [
            {
                "identifier": "aabb6d26-8a8d-4338-92c9-1b8c67ec18e0",
                "instanceIdentifier": "ebbb1400-9fe6-4566-affd-1484a1cee58f",
                "name": "GenerateFlowFile",
                "comments": "",
                "position": {
                    "x": 687.0,
                    "y": 372.0
                },
                "type": "org.apache.nifi.minifi.processors.GenerateFlowFile",
                "bundle": {
                    "group": "org.apache.nifi.minifi",
                    "artifact": "minifi-standard-processors",
                    "version": "1.23.06"
                },
                "properties": {
                    "File Size": "100B",
                    "Batch Size": "1",
                    "Unique FlowFiles": "true",
                    "Data Format": "Text"
                },
                "propertyDescriptors": {
                    "File Size": {
                        "name": "File Size",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Batch Size": {
                        "name": "Batch Size",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Unique FlowFiles": {
                        "name": "Unique FlowFiles",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Custom Text": {
                        "name": "Custom Text",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Data Format": {
                        "name": "Data Format",
                        "identifiesControllerService": false,
                        "sensitive": false
                    }
                },
                "style": {},
                "schedulingPeriod": "15 sec",
                "schedulingStrategy": "TIMER_DRIVEN",
                "executionNode": "ALL",
                "penaltyDuration": "30000 ms",
                "yieldDuration": "1000 ms",
                "bulletinLevel": "WARN",
                "runDurationMillis": 0,
                "concurrentlySchedulableTaskCount": 1,
                "autoTerminatedRelationships": [],
                "componentType": "PROCESSOR",
                "groupIdentifier": "8b4d66dc-9085-4722-b35b-3492f363baa3"
            },
            {
                "identifier": "469617f1-3898-4bbf-91fe-27d8f4dd2a75",
                "instanceIdentifier": "a43e3d65-bff9-4557-899b-43f680e4b533",
                "name": "InvokeHTTP",
                "comments": "",
                "position": {
                    "x": 1031.0,
                    "y": 374.0
                },
                "type": "org.apache.nifi.minifi.processors.InvokeHTTP",
                "bundle": {
                    "group": "org.apache.nifi.minifi",
                    "artifact": "minifi-standard-processors",
                    "version": "1.23.06"
                },
                "properties": {
                    "Proxy Host": "https://my-proxy.com",
                    "Invalid HTTP Header Field Handling Strategy": "transform",
                    "Read Timeout": "15 s",
                    "invokehttp-proxy-password": "very_secure_password",
                    "Send Message Body": "true",
                    "Proxy Port": "12345",
                    "invokehttp-proxy-username": "user",
                    "Connection Timeout": "5 s",
                    "send-message-body": "true",
                    "Content-type": "application/octet-stream",
                    "SSL Context Service": "b9801278-7b5d-4314-aed6-713fd4b5f933",
                    "Always Output Response": "false",
                    "HTTP Method": "POST",
                    "Include Date Header": "true",
                    "Use Chunked Encoding": "false",
                    "Disable Peer Verification": "false",
                    "Penalize on \"No Retry\"": "false",
                    "Follow Redirects": "true",
                    "Remote URL": "https://my-storage-server.com/postdata"
                },
                "propertyDescriptors": {
                    "Proxy Host": {
                        "name": "Proxy Host",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Attributes to Send": {
                        "name": "Attributes to Send",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Invalid HTTP Header Field Handling Strategy": {
                        "name": "Invalid HTTP Header Field Handling Strategy",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Read Timeout": {
                        "name": "Read Timeout",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "invokehttp-proxy-password": {
                        "name": "invokehttp-proxy-password",
                        "displayName": "Proxy Password",
                        "identifiesControllerService": false,
                        "sensitive": true
                    },
                    "Send Message Body": {
                        "name": "Send Message Body",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Proxy Port": {
                        "name": "Proxy Port",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "invokehttp-proxy-username": {
                        "name": "invokehttp-proxy-username",
                        "displayName": "Proxy Username",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Put Response Body in Attribute": {
                        "name": "Put Response Body in Attribute",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Connection Timeout": {
                        "name": "Connection Timeout",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "send-message-body": {
                        "name": "send-message-body",
                        "displayName": "Send Body",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Content-type": {
                        "name": "Content-type",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "SSL Context Service": {
                        "name": "SSL Context Service",
                        "identifiesControllerService": true,
                        "sensitive": false
                    },
                    "Always Output Response": {
                        "name": "Always Output Response",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "HTTP Method": {
                        "name": "HTTP Method",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Include Date Header": {
                        "name": "Include Date Header",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Use Chunked Encoding": {
                        "name": "Use Chunked Encoding",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Disable Peer Verification": {
                        "name": "Disable Peer Verification",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Penalize on \"No Retry\"": {
                        "name": "Penalize on \"No Retry\"",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Follow Redirects": {
                        "name": "Follow Redirects",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Remote URL": {
                        "name": "Remote URL",
                        "identifiesControllerService": false,
                        "sensitive": false
                    }
                },
                "style": {},
                "schedulingPeriod": "1000 ms",
                "schedulingStrategy": "EVENT_DRIVEN",
                "executionNode": "ALL",
                "penaltyDuration": "30000 ms",
                "yieldDuration": "1000 ms",
                "bulletinLevel": "WARN",
                "runDurationMillis": 0,
                "concurrentlySchedulableTaskCount": 1,
                "autoTerminatedRelationships": [
                    "success",
                    "response",
                    "failure",
                    "retry",
                    "no retry"
                ],
                "componentType": "PROCESSOR",
                "groupIdentifier": "8b4d66dc-9085-4722-b35b-3492f363baa3"
            }
        ],
        "inputPorts": [],
        "outputPorts": [],
        "connections": [
            {
                "identifier": "5c3d809b-0866-4c19-8287-439e5282c9c6",
                "instanceIdentifier": "9d986de7-dd87-47ca-b3c6-46730c53f91d",
                "name": "GenerateFlowFile/success/InvokeHTTP",
                "position": {
                    "x": 0.0,
                    "y": 0.0
                },
                "source": {
                    "id": "aabb6d26-8a8d-4338-92c9-1b8c67ec18e0",
                    "type": "PROCESSOR",
                    "groupId": "8b4d66dc-9085-4722-b35b-3492f363baa3",
                    "name": "GenerateFlowFile",
                    "instanceIdentifier": "ebbb1400-9fe6-4566-affd-1484a1cee58f"
                },
                "destination": {
                    "id": "469617f1-3898-4bbf-91fe-27d8f4dd2a75",
                    "type": "PROCESSOR",
                    "groupId": "8b4d66dc-9085-4722-b35b-3492f363baa3",
                    "name": "InvokeHTTP",
                    "instanceIdentifier": "a43e3d65-bff9-4557-899b-43f680e4b533"
                },
                "labelIndex": 1,
                "zIndex": 0,
                "selectedRelationships": [
                    "success"
                ],
                "backPressureObjectThreshold": 2000,
                "backPressureDataSizeThreshold": "100 MB",
                "flowFileExpiration": "0 seconds",
                "prioritizers": [],
                "bends": [],
                "componentType": "CONNECTION",
                "groupIdentifier": "8b4d66dc-9085-4722-b35b-3492f363baa3"
            }
        ],
        "labels": [],
        "funnels": [],)";

constexpr std::string_view config_json_with_nifi_schema_part_2 = R"(
        "controllerServices": [
            {
                "identifier": "b9801278-7b5d-4314-aed6-713fd4b5f933",
                "instanceIdentifier": "61637b96-2cf7-4a9a-83b5-ea18fb205cd3",
                "name": "SSLContextService",
                "position": {
                    "x": 0.0,
                    "y": 0.0
                },
                "type": "org.apache.nifi.minifi.controllers.SSLContextService",
                "bundle": {
                    "group": "org.apache.nifi.minifi",
                    "artifact": "minifi-system",
                    "version": "1.23.06"
                },
                "properties": {
                    "Private Key": "certs/agent-key.pem",
                    "Client Certificate": "certs/agent-cert.pem",
                    "Passphrase": "very_secure_passphrase",
                    "CA Certificate": "certs/ca-cert.pem",
                    "Use System Cert Store": "false"
                },
                "propertyDescriptors": {
                    "Private Key": {
                        "name": "Private Key",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Client Certificate": {
                        "name": "Client Certificate",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Passphrase": {
                        "name": "Passphrase",
                        "identifiesControllerService": false,
                        "sensitive": true
                    },
                    "CA Certificate": {
                        "name": "CA Certificate",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Use System Cert Store": {
                        "name": "Use System Cert Store",
                        "identifiesControllerService": false,
                        "sensitive": false
                    }
                },
                "controllerServiceApis": [
                    {
                        "type": "org.apache.nifi.minifi.controllers.SSLContextService",
                        "bundle": {
                            "group": "org.apache.nifi.minifi",
                            "artifact": "minifi-system",
                            "version": "1.23.06"
                        }
                    }
                ],
                "componentType": "CONTROLLER_SERVICE",
                "groupIdentifier": "8b4d66dc-9085-4722-b35b-3492f363baa3"
            },
            {
                "identifier": "b418f4ff-e598-4ea2-921f-14f9dd864482",
                "instanceIdentifier": "dbb76c00-97ad-4b3a-bf0c-d5d1b88e79d3",
                "name": "Second SSLContextService",
                "position": {
                    "x": 0.0,
                    "y": 0.0
                },
                "type": "org.apache.nifi.minifi.controllers.SSLContextService",
                "bundle": {
                    "group": "org.apache.nifi.minifi",
                    "artifact": "minifi-system",
                    "version": "1.23.06"
                },
                "properties": {
                    "Private Key": "second_ssl_service_certs/agent-key.pem",
                    "Client Certificate": "second_ssl_service_certs/agent-cert.pem",
                    "CA Certificate": "second_ssl_service_certs/ca-cert.pem",
                    "Use System Cert Store": "false"
                },
                "propertyDescriptors": {
                    "Private Key": {
                        "name": "Private Key",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Client Certificate": {
                        "name": "Client Certificate",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Passphrase": {
                        "name": "Passphrase",
                        "identifiesControllerService": false,
                        "sensitive": true
                    },
                    "CA Certificate": {
                        "name": "CA Certificate",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Use System Cert Store": {
                        "name": "Use System Cert Store",
                        "identifiesControllerService": false,
                        "sensitive": false
                    }
                },
                "controllerServiceApis": [
                    {
                        "type": "org.apache.nifi.minifi.controllers.SSLContextService",
                        "bundle": {
                            "group": "org.apache.nifi.minifi",
                            "artifact": "minifi-system",
                            "version": "1.23.06"
                        }
                    }
                ],
                "componentType": "CONTROLLER_SERVICE",
                "groupIdentifier": "910ec043-2372-4edb-9ef4-8ce720c50685"
            }
        ],
        "variables": {},
        "componentType": "PROCESS_GROUP"
    }
}
)";

const minifi::utils::crypto::Bytes secret_key = minifi::utils::string::from_hex("75536923a75928a970077f9dae2c2b166a5413e020cb5190de4fb8edce1a38c7");
const minifi::utils::crypto::EncryptionProvider encryption_provider{secret_key};

using OverridesMap = std::unordered_map<minifi::utils::Identifier, core::flow::Overrides>;

TEST_CASE("JsonFlowSerializer can encrypt the sensitive properties") {
  ConfigurationTestController test_controller;
  core::flow::AdaptiveConfiguration json_configuration{test_controller.getContext()};

  const auto [schema, flow_definition] = GENERATE(
      std::make_tuple(core::flow::FlowSchema::getDefault(), std::string{config_json_with_default_schema}),
      std::make_tuple(core::flow::FlowSchema::getNiFiFlowJson(), minifi::utils::string::join_pack(config_json_with_nifi_schema_part_1, config_json_with_nifi_schema_part_2)));

  const auto process_group = json_configuration.getRootFromPayload(flow_definition);
  REQUIRE(process_group);

  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.Parse(flow_definition.data(), flow_definition.size());
  REQUIRE(res);
  const auto flow_serializer = core::json::JsonFlowSerializer{std::move(doc)};

  const auto processor_id = minifi::utils::Identifier::parse("469617f1-3898-4bbf-91fe-27d8f4dd2a75").value();
  const auto controller_service_id = minifi::utils::Identifier::parse("b9801278-7b5d-4314-aed6-713fd4b5f933").value();
  const auto parameter_id = minifi::utils::Identifier::parse("721e10b7-8e00-3188-9a27-476cca376978").value();

  const auto [overrides, expected_results] = GENERATE_REF(
      std::make_tuple(OverridesMap{},
          std::array{"very_secure_password", "very_secure_passphrase", "param_value_1"}),
      std::make_tuple(OverridesMap{{processor_id, core::flow::Overrides{}.add("invokehttp-proxy-password", "password123")}},
          std::array{"password123", "very_secure_passphrase", "param_value_1"}),
      std::make_tuple(OverridesMap{{controller_service_id, core::flow::Overrides{}.add("Passphrase", "speak friend and enter")}},
          std::array{"very_secure_password", "speak friend and enter", "param_value_1"}),
      std::make_tuple(OverridesMap{{processor_id, core::flow::Overrides{}.add("invokehttp-proxy-password", "password123")},
                                   {controller_service_id, core::flow::Overrides{}.add("Passphrase", "speak friend and enter")}},
          std::array{"password123", "speak friend and enter", "param_value_1"}),
      std::make_tuple(OverridesMap{{parameter_id, core::flow::Overrides{}.add("secret_parameter", "param_value_2")}},
          std::array{"very_secure_password", "very_secure_passphrase", "param_value_2"}));

  std::string config_json_encrypted = flow_serializer.serialize(*process_group, schema, encryption_provider, overrides, {});

  {
    std::regex regex{R"_("invokehttp-proxy-password": "(.*)",)_"};
    std::smatch match_results;
    CHECK(std::regex_search(config_json_encrypted, match_results, regex));

    REQUIRE(match_results.size() == 2);
    std::string encrypted_value = match_results[1];
    CHECK(minifi::utils::crypto::property_encryption::decrypt(encrypted_value, encryption_provider) == expected_results[0]);
  }

  {
    std::regex regex{R"_("Passphrase": "(.*)",)_"};
    std::smatch match_results;
    CHECK(std::regex_search(config_json_encrypted, match_results, regex));

    REQUIRE(match_results.size() == 2);
    std::string encrypted_value = match_results[1];
    CHECK(minifi::utils::crypto::property_encryption::decrypt(encrypted_value, encryption_provider) == expected_results[1]);
  }

  {
    std::regex regex{R"_("value": "(.*)")_"};
    std::smatch match_results;
    CHECK(std::regex_search(config_json_encrypted, match_results, regex));

    REQUIRE(match_results.size() == 2);
    std::string encrypted_value = match_results[1];
    CHECK(minifi::utils::crypto::property_encryption::decrypt(encrypted_value, encryption_provider) == expected_results[2]);
  }
}

TEST_CASE("JsonFlowSerializer with an override can add a new property to the flow config file") {
  ConfigurationTestController test_controller;
  core::flow::AdaptiveConfiguration json_configuration{test_controller.getContext()};

  const auto schema = core::flow::FlowSchema::getNiFiFlowJson();
  const auto config_json_with_nifi_schema = minifi::utils::string::join_pack(config_json_with_nifi_schema_part_1, config_json_with_nifi_schema_part_2);
  const auto process_group = json_configuration.getRootFromPayload(config_json_with_nifi_schema);
  REQUIRE(process_group);

  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.Parse(config_json_with_nifi_schema.data(), config_json_with_nifi_schema.size());
  REQUIRE(res);
  const auto flow_serializer = core::json::JsonFlowSerializer{std::move(doc)};

  const auto second_controller_service_id = minifi::utils::Identifier::parse("b418f4ff-e598-4ea2-921f-14f9dd864482").value();

  SECTION("with required overrides") {
    const OverridesMap overrides{{second_controller_service_id, core::flow::Overrides{}.add("Passphrase", "new passphrase")}};

    std::string config_json_encrypted = flow_serializer.serialize(*process_group, schema, encryption_provider, overrides, {});

    std::regex regex{R"_("Passphrase": "(.*)")_"};
    std::smatch match_results;

    // skip the first match
    REQUIRE(std::regex_search(config_json_encrypted.cbegin(), config_json_encrypted.cend(), match_results, regex));

    // verify the second match
    REQUIRE(std::regex_search(match_results.suffix().first, config_json_encrypted.cend(), match_results, regex));
    REQUIRE(match_results.size() == 2);
    std::string encrypted_value = match_results[1];
    CHECK(minifi::utils::crypto::property_encryption::decrypt(encrypted_value, encryption_provider) == "new passphrase");
  }

  SECTION("with optional overrides: the override is only used if the property is already in the flow config") {
    const auto first_controller_service_id = minifi::utils::Identifier::parse("b9801278-7b5d-4314-aed6-713fd4b5f933").value();
    const OverridesMap overrides{{first_controller_service_id, core::flow::Overrides{}.addOptional("Passphrase", "first new passphrase")},
                                 {second_controller_service_id, core::flow::Overrides{}.addOptional("Passphrase", "second new passphrase")}};

    std::string config_json_encrypted = flow_serializer.serialize(*process_group, schema, encryption_provider, overrides, {});

    std::regex regex{R"_("Passphrase": "(.*)")_"};
    std::smatch match_results;

    // verify the first match
    REQUIRE(std::regex_search(config_json_encrypted.cbegin(), config_json_encrypted.cend(), match_results, regex));
    REQUIRE(match_results.size() == 2);
    std::string encrypted_value = match_results[1];
    CHECK(minifi::utils::crypto::property_encryption::decrypt(encrypted_value, encryption_provider) == "first new passphrase");

    // check that there is no second match
    CHECK_FALSE(std::regex_search(match_results.suffix().first, config_json_encrypted.cend(), match_results, regex));
  }
}

TEST_CASE("The encrypted flow configuration can be decrypted with the correct key") {
  ConfigurationTestController test_controller;
  auto configuration_context = test_controller.getContext();
  configuration_context.sensitive_values_encryptor = encryption_provider;
  core::flow::AdaptiveConfiguration json_configuration_before{configuration_context};

  const auto schema = core::flow::FlowSchema::getNiFiFlowJson();
  const auto config_json_with_nifi_schema = minifi::utils::string::join_pack(config_json_with_nifi_schema_part_1, config_json_with_nifi_schema_part_2);
  const auto process_group_before = json_configuration_before.getRootFromPayload(config_json_with_nifi_schema);
  REQUIRE(process_group_before);

  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.Parse(config_json_with_nifi_schema.data(), config_json_with_nifi_schema.size());
  REQUIRE(res);
  const auto flow_serializer = core::json::JsonFlowSerializer{std::move(doc)};
  std::string config_json_encrypted = flow_serializer.serialize(*process_group_before, schema, encryption_provider, {}, {});

  core::flow::AdaptiveConfiguration json_configuration_after{configuration_context};
  const auto process_group_after = json_configuration_after.getRootFromPayload(config_json_encrypted);
  REQUIRE(process_group_after);

  const auto processor_id = minifi::utils::Identifier::parse("469617f1-3898-4bbf-91fe-27d8f4dd2a75").value();
  const auto* processor_before = process_group_before->findProcessorById(processor_id);
  REQUIRE(processor_before);
  const auto* processor_after = process_group_after->findProcessorById(processor_id);
  REQUIRE(processor_after);
  CHECK(processor_before->getProperty("HTTP Method") == processor_after->getProperty("HTTP Method"));
  CHECK(processor_before->getProperty("invokehttp-proxy-password") == processor_after->getProperty("invokehttp-proxy-password"));

  const auto controller_service_id = "b9801278-7b5d-4314-aed6-713fd4b5f933";
  const auto* const controller_service_node_before = process_group_before->findControllerService(controller_service_id);
  REQUIRE(controller_service_node_before);
  const auto controller_service_before = controller_service_node_before->getControllerServiceImplementation();
  REQUIRE(controller_service_node_before);
  const auto* const controller_service_node_after = process_group_after->findControllerService(controller_service_id);
  REQUIRE(controller_service_node_after);
  const auto controller_service_after = controller_service_node_before->getControllerServiceImplementation();
  REQUIRE(controller_service_after);
  CHECK(controller_service_before->getProperty("CA Certificate") == controller_service_after->getProperty("CA Certificate"));
  CHECK(controller_service_before->getProperty("Passphrase") == controller_service_after->getProperty("Passphrase"));

  const auto& param_contexts = json_configuration_after.getParameterContexts();
  CHECK(param_contexts.at("my-context")->getParameter("secret_parameter")->value == "param_value_1");
}

TEST_CASE("The encrypted flow configuration cannot be decrypted with an incorrect key") {
  ConfigurationTestController test_controller;
  auto configuration_context = test_controller.getContext();
  configuration_context.sensitive_values_encryptor = encryption_provider;
  core::flow::AdaptiveConfiguration json_configuration_before{configuration_context};

  const auto schema = core::flow::FlowSchema::getNiFiFlowJson();
  const auto config_json_with_nifi_schema = minifi::utils::string::join_pack(config_json_with_nifi_schema_part_1, config_json_with_nifi_schema_part_2);
  const auto process_group_before = json_configuration_before.getRootFromPayload(config_json_with_nifi_schema);
  REQUIRE(process_group_before);

  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.Parse(config_json_with_nifi_schema.data(), config_json_with_nifi_schema.size());
  REQUIRE(res);
  const auto flow_serializer = core::json::JsonFlowSerializer{std::move(doc)};
  std::string config_json_encrypted = flow_serializer.serialize(*process_group_before, schema, encryption_provider, {}, {});

  const minifi::utils::crypto::Bytes different_secret_key = minifi::utils::string::from_hex("ea55b7d0edc22280c9547e4d89712b3fae74f96d82f240a004fb9fbd0640eec7");
  configuration_context.sensitive_values_encryptor = minifi::utils::crypto::EncryptionProvider{different_secret_key};

  core::flow::AdaptiveConfiguration json_configuration_after{configuration_context};
  REQUIRE_THROWS_AS(json_configuration_after.getRootFromPayload(config_json_encrypted), minifi::utils::crypto::EncryptionError);
}

TEST_CASE("Parameter provider generated parameter context is serialized correctly") {
  ConfigurationTestController test_controller;
  auto configuration_context = test_controller.getContext();
  configuration_context.sensitive_values_encryptor = encryption_provider;
  core::flow::AdaptiveConfiguration json_configuration_before{configuration_context};

  const auto schema = core::flow::FlowSchema::getNiFiFlowJson();
  static const std::string config_json_with_nifi_schema =
      R"(
{
  "parameterProviders": [
    {
        "identifier": "d26ee5f5-0192-1000-0482-4e333725e089",
        "name": "EnvironmentVariableParameterProvider",
        "type": "EnvironmentVariableParameterProvider",
        "properties": {
          "Environment Variable Inclusion Strategy": "Comma-Separated",
          "Include Environment Variables": "MINIFI_DATA,SECRET_MINIFI_DATA",
          "Sensitive Parameter Scope": "selected",
          "Sensitive Parameter List": "SECRET_MINIFI_DATA",
          "Parameter Group Name": "environment-variable-parameter-context"
        }
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "name": "DummyProcessor",
      "identifier": "aabb6d26-8a8d-4338-92c9-1b8c67ec18e0",
      "type": "DummyProcessor",
      "scheduling strategy": "TIMER_DRIVEN",
      "scheduling period": "15 sec",
      "properties": {
        "Simple Property": "#{MINIFI_DATA}",
        "Sensitive Property": "#{SECRET_MINIFI_DATA}"
      }
    }],
    "parameterContextName": "environment-variable-parameter-context"
  }
})";

  minifi::utils::Environment::setEnvironmentVariable("MINIFI_DATA", "minifi_data_value");
  minifi::utils::Environment::setEnvironmentVariable("SECRET_MINIFI_DATA", "secret_minifi_data_value");
  const auto process_group_before = json_configuration_before.getRootFromPayload(config_json_with_nifi_schema);
  REQUIRE(process_group_before);

  std::string reserialized_config = json_configuration_before.serialize(*process_group_before);
  rapidjson::Document result_doc;
  rapidjson::ParseResult res = result_doc.Parse(reserialized_config.data(), reserialized_config.size());
  REQUIRE(res);
  REQUIRE(result_doc.HasMember("parameterContexts"));
  auto parameters = result_doc["parameterContexts"].GetArray()[0]["parameters"].GetArray();
  REQUIRE(parameters.Size() == 2);
  for (const auto& parameter : parameters) {
    std::string name = parameter["name"].GetString();
    std::string value = parameter["value"].GetString();
    if (name == "MINIFI_DATA") {
      REQUIRE(value == "minifi_data_value");
    } else if (name == "SECRET_MINIFI_DATA") {
      REQUIRE(minifi::utils::crypto::property_encryption::decrypt(value, encryption_provider) == "secret_minifi_data_value");
    }
  }
}

TEST_CASE("Parameter provider generated parameter context is not serialized if parameter context already exists") {
  ConfigurationTestController test_controller;
  auto configuration_context = test_controller.getContext();
  configuration_context.sensitive_values_encryptor = encryption_provider;
  core::flow::AdaptiveConfiguration json_configuration_before{configuration_context};

  const auto schema = core::flow::FlowSchema::getNiFiFlowJson();
  static const std::string config_json_with_nifi_schema =
      R"(
{
  "parameterProviders": [
    {
        "identifier": "d26ee5f5-0192-1000-0482-4e333725e089",
        "name": "EnvironmentVariableParameterProvider",
        "type": "EnvironmentVariableParameterProvider",
        "properties": {
          "Environment Variable Inclusion Strategy": "Comma-Separated",
          "Include Environment Variables": "MINIFI_DATA,SECRET_MINIFI_DATA",
          "Sensitive Parameter Scope": "selected",
          "Sensitive Parameter List": "SECRET_MINIFI_DATA",
          "Parameter Group Name": "environment-variable-parameter-context"
        }
    }
  ],
  "parameterContexts": [
    {
        "identifier": "123ee5f5-0192-1000-0482-4e333725e345",
        "name": "environment-variable-parameter-context",
        "description": "my parameter context",
        "parameters": [
            {
                "name": "SECRET_MINIFI_DATA",
                "description": "",
                "sensitive": true,
                "provided": true,
                "value": "old_secret_minifi_data_value"
            },
            {
                "name": "MINIFI_DATA",
                "description": "",
                "sensitive": false,
                "provided": true,
                "value": "old_minifi_data_value"
            }
        ],
        "parameterProvider": "d26ee5f5-0192-1000-0482-4e333725e089"
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "name": "DummyProcessor",
      "identifier": "aabb6d26-8a8d-4338-92c9-1b8c67ec18e0",
      "type": "DummyProcessor",
      "scheduling strategy": "TIMER_DRIVEN",
      "scheduling period": "15 sec",
      "properties": {
        "Simple Property": "#{MINIFI_DATA}",
        "Sensitive Property": "#{SECRET_MINIFI_DATA}"
      }
    }],
    "parameterContextName": "environment-variable-parameter-context"
  }
})";

  minifi::utils::Environment::setEnvironmentVariable("MINIFI_DATA", "minifi_data_value");
  minifi::utils::Environment::setEnvironmentVariable("SECRET_MINIFI_DATA", "secret_minifi_data_value");
  const auto process_group_before = json_configuration_before.getRootFromPayload(config_json_with_nifi_schema);
  REQUIRE(process_group_before);

  std::string reserialized_config = json_configuration_before.serialize(*process_group_before);
  rapidjson::Document result_doc;
  rapidjson::ParseResult res = result_doc.Parse(reserialized_config.data(), reserialized_config.size());
  REQUIRE(res);
  REQUIRE(result_doc.HasMember("parameterContexts"));
  auto parameters = result_doc["parameterContexts"].GetArray()[0]["parameters"].GetArray();
  REQUIRE(parameters.Size() == 2);
  for (const auto& parameter : parameters) {
    std::string name = parameter["name"].GetString();
    std::string value = parameter["value"].GetString();
    if (name == "MINIFI_DATA") {
      REQUIRE(value == "old_minifi_data_value");
    } else if (name == "SECRET_MINIFI_DATA") {
      REQUIRE(minifi::utils::crypto::property_encryption::decrypt(value, encryption_provider) == "old_secret_minifi_data_value");
    }
  }
}

TEST_CASE("Parameter provider generated parameter context is reserialized if Reload Values On Restart is set to true") {
  ConfigurationTestController test_controller;
  auto configuration_context = test_controller.getContext();
  configuration_context.sensitive_values_encryptor = encryption_provider;
  core::flow::AdaptiveConfiguration json_configuration_before{configuration_context};

  const auto schema = core::flow::FlowSchema::getNiFiFlowJson();
  static const std::string config_json_with_nifi_schema =
      R"(
{
  "parameterProviders": [
    {
        "identifier": "d26ee5f5-0192-1000-0482-4e333725e089",
        "name": "EnvironmentVariableParameterProvider",
        "type": "EnvironmentVariableParameterProvider",
        "properties": {
          "Environment Variable Inclusion Strategy": "Comma-Separated",
          "Include Environment Variables": "MINIFI_DATA,SECRET_MINIFI_DATA",
          "Sensitive Parameter Scope": "selected",
          "Sensitive Parameter List": "SECRET_MINIFI_DATA",
          "Parameter Group Name": "environment-variable-parameter-context",
          "Reload Values On Restart": "true"
        }
    }
  ],
  "parameterContexts": [
    {
        "identifier": "123ee5f5-0192-1000-0482-4e333725e345",
        "name": "environment-variable-parameter-context",
        "description": "my parameter context",
        "parameters": [
            {
                "name": "SECRET_MINIFI_DATA",
                "description": "",
                "sensitive": true,
                "provided": true,
                "value": "old_secret_minifi_data_value"
            },
            {
                "name": "MINIFI_DATA",
                "description": "",
                "sensitive": false,
                "provided": true,
                "value": "old_minifi_data_value"
            }
        ],
        "parameterProvider": "d26ee5f5-0192-1000-0482-4e333725e089"
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "name": "DummyProcessor",
      "identifier": "aabb6d26-8a8d-4338-92c9-1b8c67ec18e0",
      "type": "DummyProcessor",
      "scheduling strategy": "TIMER_DRIVEN",
      "scheduling period": "15 sec",
      "properties": {
        "Simple Property": "#{MINIFI_DATA}",
        "Sensitive Property": "#{SECRET_MINIFI_DATA}"
      }
    }],
    "parameterContextName": "environment-variable-parameter-context"
  }
})";

  minifi::utils::Environment::setEnvironmentVariable("MINIFI_DATA", "minifi_data_value");
  minifi::utils::Environment::setEnvironmentVariable("SECRET_MINIFI_DATA", "secret_minifi_data_value");
  const auto process_group_before = json_configuration_before.getRootFromPayload(config_json_with_nifi_schema);
  REQUIRE(process_group_before);

  std::string reserialized_config = json_configuration_before.serialize(*process_group_before);
  rapidjson::Document result_doc;
  rapidjson::ParseResult res = result_doc.Parse(reserialized_config.data(), reserialized_config.size());
  REQUIRE(res);
  REQUIRE(result_doc.HasMember("parameterContexts"));
  auto parameters = result_doc["parameterContexts"].GetArray()[0]["parameters"].GetArray();
  REQUIRE(parameters.Size() == 2);
  for (const auto& parameter : parameters) {
    std::string name = parameter["name"].GetString();
    std::string value = parameter["value"].GetString();
    if (name == "MINIFI_DATA") {
      CHECK(value == "minifi_data_value");
    } else if (name == "SECRET_MINIFI_DATA") {
      CHECK(minifi::utils::crypto::property_encryption::decrypt(value, encryption_provider) == "secret_minifi_data_value");
    }
  }
}

}  // namespace org::apache::nifi::minifi::test
