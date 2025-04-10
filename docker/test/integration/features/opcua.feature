# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

@ENABLE_OPC
Feature: Putting and fetching data to OPC UA server
  In order to send and fetch data from an OPC UA server
  As a user of MiNiFi
  I need to have PutOPCProcessor and FetchOPCProcessor

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario Outline: Create and fetch data from an OPC UA node
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "create-opc-ua-node" flow
    And a file with the content "<Value>" is present in "/tmp/input"
    And a PutOPCProcessor processor in the "create-opc-ua-node" flow
    And a FetchOPCProcessor processor in the "fetch-opc-ua-node" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "fetch-opc-ua-node" flow
    And these processor properties are set:
      | processor name    | property name               | property value                                    |
      | PutOPCProcessor   | Parent node ID              | 85                                                |
      | PutOPCProcessor   | Parent node ID type         | Int                                               |
      | PutOPCProcessor   | Target node ID              | 9999                                              |
      | PutOPCProcessor   | Target node ID type         | Int                                               |
      | PutOPCProcessor   | Target node namespace index | 1                                                 |
      | PutOPCProcessor   | Value type                  | <Value Type>                                      |
      | PutOPCProcessor   | OPC server endpoint         | opc.tcp://opcua-server-${feature_id}:4840/        |
      | PutOPCProcessor   | Target node browse name     | testnodename                                      |
      | FetchOPCProcessor | Node ID                     | 9999                                              |
      | FetchOPCProcessor | Node ID type                | Int                                               |
      | FetchOPCProcessor | Namespace index             | 1                                                 |
      | FetchOPCProcessor | OPC server endpoint         | opc.tcp://opcua-server-${feature_id}:4840/        |
      | FetchOPCProcessor | Max depth                   | 1                                                 |

    And the "success" relationship of the GetFile processor is connected to the PutOPCProcessor
    And the "success" relationship of the FetchOPCProcessor processor is connected to the PutFile

    And an OPC UA server is set up

    When all instances start up
    Then at least one flowfile with the content "<Value>" is placed in the monitored directory in less than 60 seconds

  Examples: Topic names and formats to test
    | Value Type   | Value   |
    | String       | Test    |
    | UInt32       | 42      |
    | Double       | 123.321 |
    | Boolean      | False   |

  Scenario Outline: Update and fetch data from an OPC UA node
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "update-opc-ua-node" flow
    And a file with the content "<Value>" is present in "/tmp/input"
    And a PutOPCProcessor processor in the "update-opc-ua-node" flow
    And a FetchOPCProcessor processor in the "fetch-opc-ua-node" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "fetch-opc-ua-node" flow
    And these processor properties are set:
      | processor name    | property name               | property value                                    |
      | PutOPCProcessor   | Parent node ID              | 85                                                |
      | PutOPCProcessor   | Parent node ID type         | Int                                               |
      | PutOPCProcessor   | Target node ID              | <Node ID>                                         |
      | PutOPCProcessor   | Target node ID type         | <Node ID Type>                                    |
      | PutOPCProcessor   | Target node namespace index | 1                                                 |
      | PutOPCProcessor   | Value type                  | <Value Type>                                      |
      | PutOPCProcessor   | OPC server endpoint         | opc.tcp://opcua-server-${feature_id}:4840/        |
      | PutOPCProcessor   | Target node browse name     | testnodename                                      |
      | FetchOPCProcessor | Node ID                     | <Node ID>                                         |
      | FetchOPCProcessor | Node ID type                | <Node ID Type>                                    |
      | FetchOPCProcessor | Namespace index             | 1                                                 |
      | FetchOPCProcessor | OPC server endpoint         | opc.tcp://opcua-server-${feature_id}:4840/        |
      | FetchOPCProcessor | Max depth                   | 1                                                 |

    And the "success" relationship of the GetFile processor is connected to the PutOPCProcessor
    And the "success" relationship of the FetchOPCProcessor processor is connected to the PutFile

    And an OPC UA server is set up

    When all instances start up
    Then at least one flowfile with the content "<Value>" is placed in the monitored directory in less than 60 seconds

  # Node ids starting from 51000 are pre-defined demo node ids in the test server application (server_ctt) of the open62541 docker image. There is one nodeid defined
  # for each type supported by OPC UA. These demo nodes can be used for testing purposes. "the.answer" is also a pre-defined string id for the same testing purposes.
  Examples: Topic names and formats to test
    | Node ID Type | Node ID     | Value Type   | Value       |
    | Int          | 51034       | String       | minifi-test |
    | Int          | 51001       | Boolean      | True        |
    | String       | the.answer  | Int32        | 54          |
    | Int          | 51019       | UInt32       | 123         |
    | Int          | 51031       | Double       | 66.6        |

  Scenario: Create and fetch data from an OPC UA node through secure connection
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "create-opc-ua-node" flow
    And a file with the content "Test" is present in "/tmp/input"
    And a PutOPCProcessor processor in the "create-opc-ua-node" flow
    And a FetchOPCProcessor processor in the "fetch-opc-ua-node" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "fetch-opc-ua-node" flow
    And these processor properties are set:
      | processor name    | property name                   | property value                                    |
      | PutOPCProcessor   | Parent node ID                  | 85                                                |
      | PutOPCProcessor   | Parent node ID type             | Int                                               |
      | PutOPCProcessor   | Target node ID                  | 9999                                              |
      | PutOPCProcessor   | Target node ID type             | Int                                               |
      | PutOPCProcessor   | Target node namespace index     | 1                                                 |
      | PutOPCProcessor   | Value type                      | String                                            |
      | PutOPCProcessor   | OPC server endpoint             | opc.tcp://opcua-server-${feature_id}:4840/        |
      | PutOPCProcessor   | Target node browse name         | testnodename                                      |
      | PutOPCProcessor   | Certificate path                | /tmp/resources/opcua/opcua_client_cert.der        |
      | PutOPCProcessor   | Key path                        | /tmp/resources/opcua/opcua_client_key.der         |
      | PutOPCProcessor   | Trusted server certificate path | /tmp/resources/opcua/opcua_client_cert.der        |
      | PutOPCProcessor   | Application URI                 | urn:open62541.server.application                  |
      | FetchOPCProcessor | Node ID                         | 9999                                              |
      | FetchOPCProcessor | Node ID type                    | Int                                               |
      | FetchOPCProcessor | Namespace index                 | 1                                                 |
      | FetchOPCProcessor | OPC server endpoint             | opc.tcp://opcua-server-${feature_id}:4840/        |
      | FetchOPCProcessor | Max depth                       | 1                                                 |
      | FetchOPCProcessor | Certificate path                | /tmp/resources/opcua/opcua_client_cert.der        |
      | FetchOPCProcessor | Key path                        | /tmp/resources/opcua/opcua_client_key.der         |
      | FetchOPCProcessor | Trusted server certificate path | /tmp/resources/opcua/opcua_client_cert.der        |
      | FetchOPCProcessor | Application URI                 | urn:open62541.server.application                  |

    And the "success" relationship of the GetFile processor is connected to the PutOPCProcessor
    And the "success" relationship of the FetchOPCProcessor processor is connected to the PutFile

    And an OPC UA server is set up

    When all instances start up
    Then at least one flowfile with the content "Test" is placed in the monitored directory in less than 60 seconds
    And the OPC UA server logs contain the following message: "SecureChannel opened with SecurityPolicy http://opcfoundation.org/UA/SecurityPolicy#Aes128_Sha256_RsaOaep" in less than 5 seconds

  Scenario: Create and fetch data from an OPC UA node through username and password authenticated connection
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "create-opc-ua-node" flow
    And a file with the content "Test" is present in "/tmp/input"
    And a PutOPCProcessor processor in the "create-opc-ua-node" flow
    And a FetchOPCProcessor processor in the "fetch-opc-ua-node" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "fetch-opc-ua-node" flow
    And these processor properties are set:
      | processor name    | property name               | property value                                    |
      | PutOPCProcessor   | Parent node ID              | 85                                                |
      | PutOPCProcessor   | Parent node ID type         | Int                                               |
      | PutOPCProcessor   | Target node ID              | 9999                                              |
      | PutOPCProcessor   | Target node ID type         | Int                                               |
      | PutOPCProcessor   | Target node namespace index | 1                                                 |
      | PutOPCProcessor   | Value type                  | String                                            |
      | PutOPCProcessor   | OPC server endpoint         | opc.tcp://opcua-server-${feature_id}:4840/        |
      | PutOPCProcessor   | Target node browse name     | testnodename                                      |
      | PutOPCProcessor   | Username                    | peter                                             |
      | PutOPCProcessor   | Password                    | peter123                                          |
      | FetchOPCProcessor | Node ID                     | 9999                                              |
      | FetchOPCProcessor | Node ID type                | Int                                               |
      | FetchOPCProcessor | Namespace index             | 1                                                 |
      | FetchOPCProcessor | OPC server endpoint         | opc.tcp://opcua-server-${feature_id}:4840/        |
      | FetchOPCProcessor | Max depth                   | 1                                                 |
      | FetchOPCProcessor | Username                    | peter                                             |
      | FetchOPCProcessor | Password                    | peter123                                          |

    And the "success" relationship of the GetFile processor is connected to the PutOPCProcessor
    And the "success" relationship of the FetchOPCProcessor processor is connected to the PutFile

    And an OPC UA server is set up with access control

    When all instances start up
    Then at least one flowfile with the content "Test" is placed in the monitored directory in less than 60 seconds
