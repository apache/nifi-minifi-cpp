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

  Scenario Outline: Create and fetch data from an OPC UA node
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "create-opc-ua-node" flow
    And a directory at "/tmp/input" has a file with the content "<Value>" in the "create-opc-ua-node" flow
    And a PutOPCProcessor processor in the "create-opc-ua-node" flow
    And PutOPCProcessor is EVENT_DRIVEN in the "create-opc-ua-node" flow
    And a FetchOPCProcessor processor in the "fetch-opc-ua-node" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "fetch-opc-ua-node" flow
    And PutFile's success relationship is auto-terminated in the "fetch-opc-ua-node" flow
    And PutFile is EVENT_DRIVEN in the "fetch-opc-ua-node" flow
    And these processor properties are set in the "create-opc-ua-node" flow
      | processor name    | property name               | property value                                    |
      | PutOPCProcessor   | Parent node ID              | 85                                                |
      | PutOPCProcessor   | Parent node ID type         | Int                                               |
      | PutOPCProcessor   | Target node ID              | 9999                                              |
      | PutOPCProcessor   | Target node ID type         | Int                                               |
      | PutOPCProcessor   | Target node namespace index | 1                                                 |
      | PutOPCProcessor   | Value type                  | <Value Type>                                      |
      | PutOPCProcessor   | OPC server endpoint         | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | PutOPCProcessor   | Target node browse name     | testnodename                                      |
    And these processor properties are set in the "fetch-opc-ua-node" flow
      | processor name    | property name               | property value                                    |
      | FetchOPCProcessor | Node ID                     | 9999                                              |
      | FetchOPCProcessor | Node ID type                | Int                                               |
      | FetchOPCProcessor | Namespace index             | 1                                                 |
      | FetchOPCProcessor | OPC server endpoint         | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | FetchOPCProcessor | Max depth                   | 1                                                 |

    And in the "create-opc-ua-node" flow the "success" relationship of the GetFile processor is connected to the PutOPCProcessor
    And in the "fetch-opc-ua-node" flow the "success" relationship of the FetchOPCProcessor processor is connected to the PutFile

    And an OPC UA server is set up

    When all instances start up
    Then in the "fetch-opc-ua-node" container at least one file with the content "<Value>" is placed in the "/tmp/output" directory in less than 60 seconds

  Examples: Topic names and formats to test
    | Value Type   | Value   |
    | String       | Test    |
    | UInt32       | 42      |
    | Double       | 123.321 |
    | Boolean      | False   |

  # The target node does not exist on the server, so the first flow file creates it and the second
  # one - dropped into the input directory only after the first value has been read back - takes the
  # update path. Sequencing the two writes through separate steps is what makes this deterministic:
  # if both files were present at startup, GetFile's listing order would decide which value lands
  # last and the final assertion would be a coin flip.
  Scenario Outline: Update and fetch data from an OPC UA node
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "update-opc-ua-node" flow
    And a directory at "/tmp/input" has a file with the content "<Initial Value>" in the "update-opc-ua-node" flow
    And a PutOPCProcessor processor in the "update-opc-ua-node" flow
    And PutOPCProcessor is EVENT_DRIVEN in the "update-opc-ua-node" flow
    And PutOPCProcessor's success relationship is auto-terminated in the "update-opc-ua-node" flow
    And a FetchOPCProcessor processor in the "fetch-opc-ua-node" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "fetch-opc-ua-node" flow
    And PutFile's success relationship is auto-terminated in the "fetch-opc-ua-node" flow
    And PutFile is EVENT_DRIVEN in the "fetch-opc-ua-node" flow
    And these processor properties are set in the "update-opc-ua-node" flow
      | processor name    | property name               | property value                                    |
      | PutOPCProcessor   | Parent node ID              | 85                                                |
      | PutOPCProcessor   | Parent node ID type         | Int                                               |
      | PutOPCProcessor   | Target node ID              | <Node ID>                                         |
      | PutOPCProcessor   | Target node ID type         | <Node ID Type>                                    |
      | PutOPCProcessor   | Target node namespace index | 1                                                 |
      | PutOPCProcessor   | Value type                  | <Value Type>                                      |
      | PutOPCProcessor   | OPC server endpoint         | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | PutOPCProcessor   | Target node browse name     | updatetestnode                                    |
    And these processor properties are set in the "fetch-opc-ua-node" flow
      | processor name    | property name               | property value                                    |
      | FetchOPCProcessor | Node ID                     | <Node ID>                                         |
      | FetchOPCProcessor | Node ID type                | <Node ID Type>                                    |
      | FetchOPCProcessor | Namespace index             | 1                                                 |
      | FetchOPCProcessor | OPC server endpoint         | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | FetchOPCProcessor | Max depth                   | 1                                                 |

    And in the "update-opc-ua-node" flow the "success" relationship of the GetFile processor is connected to the PutOPCProcessor
    And in the "fetch-opc-ua-node" flow the "success" relationship of the FetchOPCProcessor processor is connected to the PutFile

    And an OPC UA server is set up

    When all instances start up
    # The node did not exist, so this first value proves the node was created.
    Then in the "fetch-opc-ua-node" container at least one file with the content "<Initial Value>" is placed in the "/tmp/output" directory in less than 60 seconds

    # Only now, with the node known to exist, write a second value to the same node. Reading this
    # one back is only possible if PutOPCProcessor can update a node it created itself.
    When a file with the content "<Updated Value>" is placed in "/tmp/input" in the "update-opc-ua-node" flow
    Then in the "fetch-opc-ua-node" container at least one file with the content "<Updated Value>" is placed in the "/tmp/output" directory in less than 60 seconds
    And the logs of the "update-opc-ua-node" container do not contain the following message: "Failed to update node" after 1 second

  Examples: Node id types and value types to test
    | Node ID Type | Node ID     | Value Type   | Initial Value | Updated Value |
    | Int          | 9001        | String       | minifi-test   | minifi-update |
    | Int          | 9002        | Boolean      | False         | True          |
    | String       | updatetest  | Int32        | 54            | 55            |
    | Int          | 9003        | UInt32       | 123           | 456           |
    | Int          | 9004        | Double       | 66.6          | 77.7          |

  Scenario: Create and fetch data from an OPC UA node through secure connection
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "create-opc-ua-node" flow
    And a directory at "/tmp/input" has a file with the content "Test" in the "create-opc-ua-node" flow
    And a PutOPCProcessor processor in the "create-opc-ua-node" flow
    And PutOPCProcessor is EVENT_DRIVEN in the "create-opc-ua-node" flow
    And a FetchOPCProcessor processor in the "fetch-opc-ua-node" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "fetch-opc-ua-node" flow
    And PutFile's success relationship is auto-terminated in the "fetch-opc-ua-node" flow
    And PutFile is EVENT_DRIVEN in the "fetch-opc-ua-node" flow
    And the OPC UA server certificate files are placed in the "/tmp/resources/opcua/" directory in the MiNiFi container "create-opc-ua-node"
    And the OPC UA server certificate files are placed in the "/tmp/resources/opcua/" directory in the MiNiFi container "fetch-opc-ua-node"
    And these processor properties are set in the "create-opc-ua-node" flow
      | processor name    | property name                   | property value                                    |
      | PutOPCProcessor   | Parent node ID                  | 85                                                |
      | PutOPCProcessor   | Parent node ID type             | Int                                               |
      | PutOPCProcessor   | Target node ID                  | 9999                                              |
      | PutOPCProcessor   | Target node ID type             | Int                                               |
      | PutOPCProcessor   | Target node namespace index     | 1                                                 |
      | PutOPCProcessor   | Value type                      | String                                            |
      | PutOPCProcessor   | OPC server endpoint             | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | PutOPCProcessor   | Target node browse name         | testnodename                                      |
      | PutOPCProcessor   | Certificate path                | /tmp/resources/opcua/opcua_client_cert.der        |
      | PutOPCProcessor   | Key path                        | /tmp/resources/opcua/opcua_client_key.der         |
      | PutOPCProcessor   | Trusted server certificate path | /tmp/resources/opcua/opcua_client_cert.der        |
      | PutOPCProcessor   | Application URI                 | urn:open62541.unconfigured.application                  |
    And these processor properties are set in the "fetch-opc-ua-node" flow
      | processor name    | property name                   | property value                                    |
      | FetchOPCProcessor | Node ID                         | 9999                                              |
      | FetchOPCProcessor | Node ID type                    | Int                                               |
      | FetchOPCProcessor | Namespace index                 | 1                                                 |
      | FetchOPCProcessor | OPC server endpoint             | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | FetchOPCProcessor | Max depth                       | 1                                                 |
      | FetchOPCProcessor | Certificate path                | /tmp/resources/opcua/opcua_client_cert.der        |
      | FetchOPCProcessor | Key path                        | /tmp/resources/opcua/opcua_client_key.der         |
      | FetchOPCProcessor | Trusted server certificate path | /tmp/resources/opcua/opcua_client_cert.der        |
      | FetchOPCProcessor | Application URI                 | urn:open62541.unconfigured.application                  |

    And in the "create-opc-ua-node" flow the "success" relationship of the GetFile processor is connected to the PutOPCProcessor
    And in the "fetch-opc-ua-node" flow the "success" relationship of the FetchOPCProcessor processor is connected to the PutFile

    And an OPC UA server is set up

    When all instances start up

    Then in the "fetch-opc-ua-node" container at least one file with the content "Test" is placed in the "/tmp/output" directory in less than 60 seconds
    And the OPC UA server logs contain the following message: "Channel opened with SecurityMode SignAndEncrypt for SecurityPolicy http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss" in less than 5 seconds

  Scenario: Create and fetch data from an OPC UA node through username and password authenticated connection
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "create-opc-ua-node" flow
    And a directory at "/tmp/input" has a file with the content "Test" in the "create-opc-ua-node" flow
    And a PutOPCProcessor processor in the "create-opc-ua-node" flow
    And PutOPCProcessor is EVENT_DRIVEN in the "create-opc-ua-node" flow
    And a FetchOPCProcessor processor in the "fetch-opc-ua-node" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "fetch-opc-ua-node" flow
    And PutFile's success relationship is auto-terminated in the "fetch-opc-ua-node" flow
    And PutFile is EVENT_DRIVEN in the "fetch-opc-ua-node" flow
    And these processor properties are set in the "create-opc-ua-node" flow
      | processor name    | property name               | property value                                    |
      | PutOPCProcessor   | Parent node ID              | 85                                                |
      | PutOPCProcessor   | Parent node ID type         | Int                                               |
      | PutOPCProcessor   | Target node ID              | 9999                                              |
      | PutOPCProcessor   | Target node ID type         | Int                                               |
      | PutOPCProcessor   | Target node namespace index | 1                                                 |
      | PutOPCProcessor   | Value type                  | String                                            |
      | PutOPCProcessor   | OPC server endpoint         | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | PutOPCProcessor   | Target node browse name     | testnodename                                      |
      | PutOPCProcessor   | Username                    | peter                                             |
      | PutOPCProcessor   | Password                    | peter123                                          |
    And these processor properties are set in the "fetch-opc-ua-node" flow
      | processor name    | property name               | property value                                    |
      | FetchOPCProcessor | Node ID                     | 9999                                              |
      | FetchOPCProcessor | Node ID type                | Int                                               |
      | FetchOPCProcessor | Namespace index             | 1                                                 |
      | FetchOPCProcessor | OPC server endpoint         | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | FetchOPCProcessor | Max depth                   | 1                                                 |
      | FetchOPCProcessor | Username                    | peter                                             |
      | FetchOPCProcessor | Password                    | peter123                                          |

    And in the "create-opc-ua-node" flow the "success" relationship of the GetFile processor is connected to the PutOPCProcessor
    And in the "fetch-opc-ua-node" flow the "success" relationship of the FetchOPCProcessor processor is connected to the PutFile

    And an OPC UA server is set up with access control

    When all instances start up
    Then in the "fetch-opc-ua-node" container at least one file with the content "Test" is placed in the "/tmp/output" directory in less than 60 seconds
    And the logs of the "fetch-opc-ua-node" container contain the following message: "Username/password authentication is used without encryption, which is not secure. Please consider configuring encryption for better security." in less than 1 second
    And the logs of the "create-opc-ua-node" container contain the following message: "Username/password authentication is used without encryption, which is not secure. Please consider configuring encryption for better security." in less than 1 second

  Scenario: Create and fetch data from an OPC UA node through username and password authenticated connection with encryption
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "create-opc-ua-node" flow
    And a directory at "/tmp/input" has a file with the content "Test" in the "create-opc-ua-node" flow
    And a PutOPCProcessor processor in the "create-opc-ua-node" flow
    And PutOPCProcessor is EVENT_DRIVEN in the "create-opc-ua-node" flow
    And a FetchOPCProcessor processor in the "fetch-opc-ua-node" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "fetch-opc-ua-node" flow
    And PutFile's success relationship is auto-terminated in the "fetch-opc-ua-node" flow
    And PutFile is EVENT_DRIVEN in the "fetch-opc-ua-node" flow
    And the OPC UA server certificate files are placed in the "/tmp/resources/opcua/" directory in the MiNiFi container "create-opc-ua-node"
    And the OPC UA server certificate files are placed in the "/tmp/resources/opcua/" directory in the MiNiFi container "fetch-opc-ua-node"
    And these processor properties are set in the "create-opc-ua-node" flow
      | processor name    | property name                   | property value                                    |
      | PutOPCProcessor   | Parent node ID                  | 85                                                |
      | PutOPCProcessor   | Parent node ID type             | Int                                               |
      | PutOPCProcessor   | Target node ID                  | 9999                                              |
      | PutOPCProcessor   | Target node ID type             | Int                                               |
      | PutOPCProcessor   | Target node namespace index     | 1                                                 |
      | PutOPCProcessor   | Value type                      | String                                            |
      | PutOPCProcessor   | OPC server endpoint             | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | PutOPCProcessor   | Target node browse name         | testnodename                                      |
      | PutOPCProcessor   | Username                        | admin                                             |
      | PutOPCProcessor   | Password                        | admin                                             |
      | PutOPCProcessor   | Certificate path                | /tmp/resources/opcua/opcua_client_cert.der        |
      | PutOPCProcessor   | Key path                        | /tmp/resources/opcua/opcua_client_key.der         |
      | PutOPCProcessor   | Trusted server certificate path | /tmp/resources/opcua/opcua_client_cert.der        |
      | PutOPCProcessor   | Application URI                 | urn:open62541.unconfigured.application            |
    And these processor properties are set in the "fetch-opc-ua-node" flow
      | processor name    | property name                   | property value                                    |
      | FetchOPCProcessor | Node ID                         | 9999                                              |
      | FetchOPCProcessor | Node ID type                    | Int                                               |
      | FetchOPCProcessor | Namespace index                 | 1                                                 |
      | FetchOPCProcessor | OPC server endpoint             | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | FetchOPCProcessor | Max depth                       | 1                                                 |
      | FetchOPCProcessor | Username                        | admin                                             |
      | FetchOPCProcessor | Password                        | admin                                             |
      | FetchOPCProcessor | Certificate path                | /tmp/resources/opcua/opcua_client_cert.der        |
      | FetchOPCProcessor | Key path                        | /tmp/resources/opcua/opcua_client_key.der         |
      | FetchOPCProcessor | Trusted server certificate path | /tmp/resources/opcua/opcua_client_cert.der        |
      | FetchOPCProcessor | Application URI                 | urn:open62541.unconfigured.application            |

    And in the "create-opc-ua-node" flow the "success" relationship of the GetFile processor is connected to the PutOPCProcessor
    And in the "fetch-opc-ua-node" flow the "success" relationship of the FetchOPCProcessor processor is connected to the PutFile

    And an OPC UA server is set up

    When all instances start up
    Then in the "fetch-opc-ua-node" container at least one file with the content "Test" is placed in the "/tmp/output" directory in less than 60 seconds
    And the logs of the "fetch-opc-ua-node" container do not contain the following message: "Username/password authentication is used without encryption, which is not secure. Please consider configuring encryption for better security." after 0 seconds
    And the logs of the "create-opc-ua-node" container do not contain the following message: "Username/password authentication is used without encryption, which is not secure. Please consider configuring encryption for better security." after 0 seconds

  Scenario: Update and fetch historical data from an OPC UA node
    Given a GenerateFlowFile processor with the "Unique FlowFiles" property set to "false" in the "update-opc-ua-node" flow
    And the "Custom Text" property of the GenerateFlowFile processor is set to "${nextInt()}" in the "update-opc-ua-node" flow
    And the "Data Format" property of the GenerateFlowFile processor is set to "Text" in the "update-opc-ua-node" flow
    And the scheduling period of the GenerateFlowFile processor is set to "1 sec" in the "update-opc-ua-node" flow
    And a PutOPCProcessor processor in the "update-opc-ua-node" flow
    And PutOPCProcessor is EVENT_DRIVEN in the "update-opc-ua-node" flow
    And PutOPCProcessor's success relationship is auto-terminated in the "update-opc-ua-node" flow
    And a FetchOPCHistory processor in the "fetch-opc-ua-node-history" flow
    And the scheduling period of the FetchOPCHistory processor is set to "10 sec" in the "fetch-opc-ua-node-history" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "fetch-opc-ua-node-history" flow
    And PutFile's success relationship is auto-terminated in the "fetch-opc-ua-node-history" flow
    And PutFile is EVENT_DRIVEN in the "fetch-opc-ua-node-history" flow
    And a LogAttribute processor in the "fetch-opc-ua-node-history" flow
    And LogAttribute is EVENT_DRIVEN in the "fetch-opc-ua-node-history" flow
    And the "Log Payload" property of the LogAttribute processor is set to "true" in the "fetch-opc-ua-node-history" flow
    And LogAttribute's success relationship is auto-terminated in the "fetch-opc-ua-node-history" flow
    And these processor properties are set in the "update-opc-ua-node" flow
      | processor name    | property name               | property value                                    |
      | PutOPCProcessor   | Parent node ID              | 85                                                |
      | PutOPCProcessor   | Parent node ID type         | Int                                               |
      | PutOPCProcessor   | Target node ID              | myUintValue                                       |
      | PutOPCProcessor   | Target node ID type         | String                                            |
      | PutOPCProcessor   | Target node namespace index | 1                                                 |
      | PutOPCProcessor   | Value type                  | UInt32                                            |
      | PutOPCProcessor   | OPC server endpoint         | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | PutOPCProcessor   | Target node browse name     | testnodename                                      |
    And these processor properties are set in the "fetch-opc-ua-node-history" flow
      | processor name    | property name               | property value                                    |
      | FetchOPCHistory   | Node ID                     | myUintValue                                       |
      | FetchOPCHistory   | Node ID type                | String                                            |
      | FetchOPCHistory   | Namespace index             | 1                                                 |
      | FetchOPCHistory   | OPC server endpoint         | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | FetchOPCHistory   | Batch Size                  | 3                                                 |
      | FetchOPCHistory   | History Read Type           | Raw                                               |

    And in the "update-opc-ua-node" flow the "success" relationship of the GenerateFlowFile processor is connected to the PutOPCProcessor
    And in the "fetch-opc-ua-node-history" flow the "success" relationship of the FetchOPCHistory processor is connected to the PutFile
    And in the "fetch-opc-ua-node-history" flow the "success" relationship of the PutFile processor is connected to the LogAttribute

    And an OPC UA server is set up with historical data support

    When all instances start up
    Then in the "fetch-opc-ua-node-history" container files with at least these contents "0,1,2,3,4" are placed in the "/tmp/output" directory in less than 30 seconds
    And the logs of the "fetch-opc-ua-node-history" container contain the following message: "key:Namespace index value:1" in less than 10 seconds
    And the logs of the "fetch-opc-ua-node-history" container contain the following message: "key:NodeID value:myUintValue" in less than 10 seconds
    And the logs of the "fetch-opc-ua-node-history" container contain the following message: "key:Sourcetimestamp value:" in less than 10 seconds

  Scenario: Update and fetch historical data from an OPC UA node in JSON format
    Given a JsonRecordSetWriter controller service is set up in the "fetch-opc-ua-node-history" flow
    And a GenerateFlowFile processor with the "Unique FlowFiles" property set to "false" in the "update-opc-ua-node" flow
    And the "Custom Text" property of the GenerateFlowFile processor is set to "${nextInt()}" in the "update-opc-ua-node" flow
    And the "Data Format" property of the GenerateFlowFile processor is set to "Text" in the "update-opc-ua-node" flow
    And the scheduling period of the GenerateFlowFile processor is set to "1 sec" in the "update-opc-ua-node" flow
    And a PutOPCProcessor processor in the "update-opc-ua-node" flow
    And PutOPCProcessor is EVENT_DRIVEN in the "update-opc-ua-node" flow
    And PutOPCProcessor's success relationship is auto-terminated in the "update-opc-ua-node" flow
    And a FetchOPCHistory processor in the "fetch-opc-ua-node-history" flow
    And the scheduling period of the FetchOPCHistory processor is set to "10 sec" in the "fetch-opc-ua-node-history" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "fetch-opc-ua-node-history" flow
    And PutFile's success relationship is auto-terminated in the "fetch-opc-ua-node-history" flow
    And PutFile is EVENT_DRIVEN in the "fetch-opc-ua-node-history" flow
    And a LogAttribute processor in the "fetch-opc-ua-node-history" flow
    And LogAttribute is EVENT_DRIVEN in the "fetch-opc-ua-node-history" flow
    And the "Log Payload" property of the LogAttribute processor is set to "true" in the "fetch-opc-ua-node-history" flow
    And LogAttribute's success relationship is auto-terminated in the "fetch-opc-ua-node-history" flow
    And these processor properties are set in the "update-opc-ua-node" flow
      | processor name    | property name               | property value                                    |
      | PutOPCProcessor   | Parent node ID              | 85                                                |
      | PutOPCProcessor   | Parent node ID type         | Int                                               |
      | PutOPCProcessor   | Target node ID              | myUintValue                                       |
      | PutOPCProcessor   | Target node ID type         | String                                            |
      | PutOPCProcessor   | Target node namespace index | 1                                                 |
      | PutOPCProcessor   | Value type                  | UInt32                                            |
      | PutOPCProcessor   | OPC server endpoint         | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | PutOPCProcessor   | Target node browse name     | testnodename                                      |
    And these processor properties are set in the "fetch-opc-ua-node-history" flow
      | processor name    | property name               | property value                                    |
      | FetchOPCHistory   | Node ID                     | myUintValue                                       |
      | FetchOPCHistory   | Node ID type                | String                                            |
      | FetchOPCHistory   | Namespace index             | 1                                                 |
      | FetchOPCHistory   | OPC server endpoint         | opc.tcp://opcua-server-${scenario_id}:4840/       |
      | FetchOPCHistory   | Batch Size                  | 3                                                 |
      | FetchOPCHistory   | Record Set Writer           | JsonRecordSetWriter                               |

    And in the "update-opc-ua-node" flow the "success" relationship of the GenerateFlowFile processor is connected to the PutOPCProcessor
    And in the "fetch-opc-ua-node-history" flow the "success" relationship of the FetchOPCHistory processor is connected to the PutFile
    And in the "fetch-opc-ua-node-history" flow the "success" relationship of the PutFile processor is connected to the LogAttribute

    And an OPC UA server is set up with historical data support

    When all instances start up
    Then in the 'fetch-opc-ua-node-history' container at least one file in '/tmp/output' content match the following regex: '\[\{"Sourcetimestamp":".+","NodeID":"myUintValue","Namespace index":"1","Value":"0"\},\{"Sourcetimestamp":".+","NodeID":"myUintValue","Namespace index":"1","Value":"1"\},\{"Sourcetimestamp":".+","NodeID":"myUintValue","Namespace index":"1","Value":"2"\}\]' in less than 30 seconds
