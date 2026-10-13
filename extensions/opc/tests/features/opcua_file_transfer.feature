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
Feature: Transferring files from an OPC UA server
  In order to read files exposed by an OPC UA server
  As a user of MiNiFi
  I need to have FetchOPCFile processor

  Scenario Outline: Fetch an existing file from an OPC UA server
    Given the OPC UA file server has a file "hello.txt" with the content "Hello OPC UA files"
    And a FetchOPCFile processor
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And PutFile's success relationship is auto-terminated
    And these processor properties are set
      | processor name | property name        | property value                                   |
      | FetchOPCFile   | OPC server endpoint  | opc.tcp://opcua-file-server-${scenario_id}:4840/ |
      | FetchOPCFile   | Node ID type         | <Node ID Type>                                   |
      | FetchOPCFile   | Node ID              | <Node ID>                                        |
      | FetchOPCFile   | Namespace index      | 1                                                |
    And the "success" relationship of the FetchOPCFile processor is connected to the PutFile
    And an OPC UA file server is set up

    When all instances start up
    Then at least one file with the content "Hello OPC UA files" is placed in the "/tmp/output" directory in less than 60 seconds

  Examples: Node ID types
    | Node ID Type | Node ID         |
    | String       | hello.txt       |
    | Path         | Files/hello.txt |
