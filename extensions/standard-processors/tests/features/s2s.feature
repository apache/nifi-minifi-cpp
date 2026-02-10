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

@CORE
Feature: Sending data from MiNiFi-C++ to NiFi using S2S protocol
  In order to transfer data inbetween NiFi and MiNiFi flows
  As a user of MiNiFi
  I need to have RemoteProcessGroup flow nodes

  Scenario: A MiNiFi instance produces and transfers data to a NiFi instance via s2s
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "http://nifi-${scenario_id}:8080/nifi"
    And an input port with name "to_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And the "success" relationship of the GetFile processor is connected to the to_nifi

    And a NiFi container is set up
    And a NiFi flow is receiving data from the RemoteProcessGroup named "RemoteProcessGroup" in an input port named "from-minifi" which has the same id as the port named "to_nifi"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And in the "nifi" flow the "success" relationship of the from-minifi node is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "nifi" flow
    And PutFile's failure relationship is auto-terminated in the "nifi" flow

    When NiFi is started
    And all instances start up

    Then in the "nifi" container a single file with the content "test" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds

  Scenario: A MiNiFi instance produces and transfers a large data file to a NiFi instance via s2s
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "this is a very long file we want to send by site-to-site"
    And a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "http://nifi-${scenario_id}:8080/nifi"
    And an input port with name "to_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And the "success" relationship of the GetFile processor is connected to the to_nifi

    And a NiFi container is set up
    And a NiFi flow is receiving data from the RemoteProcessGroup named "RemoteProcessGroup" in an input port named "from-minifi" which has the same id as the port named "to_nifi"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And in the "nifi" flow the "success" relationship of the from-minifi node is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "nifi" flow
    And PutFile's failure relationship is auto-terminated in the "nifi" flow

    When NiFi is started
    And all instances start up

    Then in the "nifi" container a single file with the content "this is a very long file we want to send by site-to-site" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds

  Scenario: Zero length files are transfered between via s2s if the "drop empty" connection property is false
    # "drop empty" is only supported with yaml config
    Given a MiNiFi CPP server with yaml config
    And a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "http://nifi-${scenario_id}:8080/nifi"
    And an input port with name "to_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And the "success" relationship of the GenerateFlowFile processor is connected to the to_nifi

    And a NiFi container is set up
    And a NiFi flow is receiving data from the RemoteProcessGroup named "RemoteProcessGroup" in an input port named "from-minifi" which has the same id as the port named "to_nifi"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And in the "nifi" flow the "success" relationship of the from-minifi node is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "nifi" flow
    And PutFile's failure relationship is auto-terminated in the "nifi" flow

    When NiFi is started
    And all instances start up

    Then in the "nifi" container at least one empty file is placed in the "/tmp/output" directory in less than 90 seconds

  Scenario: Zero length files are not transfered between via s2s if the "drop empty" connection property is true
    # "drop empty" is only supported with yaml config
    Given a MiNiFi CPP server with yaml config
    And a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "http://nifi-${scenario_id}:8080/nifi"
    And an input port with name "to_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And the "success" relationship of the GenerateFlowFile processor is connected to the to_nifi
    And the connection going to to_nifi has "drop empty" set

    And a NiFi container is set up
    And a NiFi flow is receiving data from the RemoteProcessGroup named "RemoteProcessGroup" in an input port named "from-minifi" which has the same id as the port named "to_nifi"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And in the "nifi" flow the "success" relationship of the from-minifi node is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "nifi" flow
    And PutFile's failure relationship is auto-terminated in the "nifi" flow

    When NiFi is started
    And all instances start up

    Then in the "nifi" container no files are placed in the "/tmp/output" directory in 50 seconds of running time

  Scenario: A MiNiFi instance produces and transfers data to a NiFi instance via s2s using SSL
    Given an ssl context service is set up
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a directory at "/tmp/input" has a file with the content "test"
    And a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "https://nifi-${scenario_id}:8443/nifi"
    And an input port with name "to_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And the "SSL Context Service" property of the "to_nifi" port in the "RemoteProcessGroup" remote process group is set to "SSLContextService"
    And the "success" relationship of the GetFile processor is connected to the to_nifi

    And a NiFi container is set up with SSL enabled
    And a NiFi flow is receiving data from the RemoteProcessGroup named "RemoteProcessGroup" in an input port named "from-minifi" which has the same id as the port named "to_nifi"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And in the "nifi" flow the "success" relationship of the from-minifi node is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "nifi" flow
    And PutFile's failure relationship is auto-terminated in the "nifi" flow

    When NiFi is started
    And all instances start up

    Then in the "nifi" container a single file with the content "test" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds

  Scenario: A MiNiFi instance produces and transfers data to a NiFi instance via s2s using SSL config defined in minifi.properties
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a directory at "/tmp/input" has a file with the content "test"
    And a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "https://nifi-${scenario_id}:8443/nifi"
    And an input port with name "to_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And the "success" relationship of the GetFile processor is connected to the to_nifi
    And SSL properties are set in MiNiFi

    And a NiFi container is set up with SSL enabled
    And a NiFi flow is receiving data from the RemoteProcessGroup named "RemoteProcessGroup" in an input port named "from-minifi" which has the same id as the port named "to_nifi"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And in the "nifi" flow the "success" relationship of the from-minifi node is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "nifi" flow
    And PutFile's failure relationship is auto-terminated in the "nifi" flow

    When NiFi is started
    And all instances start up

    Then in the "nifi" container a single file with the content "test" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds

  Scenario: A MiNiFi instance produces and transfers data to a NiFi instance via s2s using HTTP protocol
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "http://nifi-${scenario_id}:8080/nifi" with transport protocol set to "HTTP"
    And an input port with name "to_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And the "success" relationship of the GetFile processor is connected to the to_nifi

    And a NiFi container is set up
    And a NiFi flow is receiving data from the RemoteProcessGroup named "RemoteProcessGroup" in an input port named "from-minifi" which has the same id as the port named "to_nifi"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And in the "nifi" flow the "success" relationship of the from-minifi node is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "nifi" flow
    And PutFile's failure relationship is auto-terminated in the "nifi" flow

    When NiFi is started
    And all instances start up

    Then in the "nifi" container a single file with the content "test" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds
    And the Minifi logs do not contain the following message: "response code 500" after 1 seconds

  Scenario: A NiFi instance produces and transfers data to a MiNiFi instance via s2s
    Given a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "http://nifi-${scenario_id}:8080/nifi"
    And an output port with name "from_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the output port "from_nifi" is connected to the PutFile processor
    And PutFile's success relationship is auto-terminated

    And a NiFi container is set up
    And a directory at "/tmp/input" has a file with the content "test" in the "nifi" flow
    And a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "nifi" flow
    And a NiFi flow is sending data to an output port named "to-minifi-in-nifi" with the id of the port named "from_nifi" from the RemoteProcessGroup named "RemoteProcessGroup"
    And in the "nifi" flow the "success" relationship of the GetFile processor is connected to the to-minifi-in-nifi

    When NiFi is started
    And all instances start up

    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds

  Scenario: A NiFi instance produces and transfers data to a MiNiFi instance via s2s using HTTP protocol
    Given a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "http://nifi-${scenario_id}:8080/nifi" with transport protocol set to "HTTP"
    And an output port with name "from_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the output port "from_nifi" is connected to the PutFile processor
    And PutFile's success relationship is auto-terminated

    And a NiFi container is set up
    And a directory at "/tmp/input" has a file with the content "test" in the "nifi" flow
    And a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "nifi" flow
    And a NiFi flow is sending data to an output port named "to-minifi-in-nifi" with the id of the port named "from_nifi" from the RemoteProcessGroup named "RemoteProcessGroup"
    And in the "nifi" flow the "success" relationship of the GetFile processor is connected to the to-minifi-in-nifi

    When NiFi is started
    And all instances start up

    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds
    And the Minifi logs do not contain the following message: "response code 500" after 1 seconds

  Scenario: A NiFi instance produces and transfers data to a MiNiFi instance via s2s with SSL config defined in minifi.properties
    Given a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "https://nifi-${scenario_id}:8443/nifi"
    And an output port with name "from_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the output port "from_nifi" is connected to the PutFile processor
    And SSL properties are set in MiNiFi
    And PutFile's success relationship is auto-terminated

    And a NiFi container is set up with SSL enabled
    And a directory at "/tmp/input" has a file with the content "test" in the "nifi" flow
    And a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "nifi" flow
    And a NiFi flow is sending data to an output port named "to-minifi-in-nifi" with the id of the port named "from_nifi" from the RemoteProcessGroup named "RemoteProcessGroup"
    And in the "nifi" flow the "success" relationship of the GetFile processor is connected to the to-minifi-in-nifi

    When NiFi is started
    And all instances start up

    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds

  Scenario: A NiFi instance produces and transfers data to a MiNiFi instance via s2s using HTTP protocol with SSL config defined in minifi.properties
    Given a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "https://nifi-${scenario_id}:8443/nifi" with transport protocol set to "HTTP"
    And an output port with name "from_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the output port "from_nifi" is connected to the PutFile processor
    And SSL properties are set in MiNiFi
    And PutFile's success relationship is auto-terminated

    And a NiFi container is set up with SSL enabled
    And a directory at "/tmp/input" has a file with the content "test" in the "nifi" flow
    And a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "nifi" flow
    And a NiFi flow is sending data to an output port named "to-minifi-in-nifi" with the id of the port named "from_nifi" from the RemoteProcessGroup named "RemoteProcessGroup"
    And in the "nifi" flow the "success" relationship of the GetFile processor is connected to the to-minifi-in-nifi

    When NiFi is started
    And all instances start up

    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds
    And the Minifi logs do not contain the following message: "response code 500" after 1 seconds

  Scenario: A MiNiFi instance produces and transfers data to a NiFi instance via s2s using compression
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "http://nifi-${scenario_id}:8080/nifi"
    And an input port using compression with name "to_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And the "success" relationship of the GetFile processor is connected to the to_nifi

    And a NiFi container is set up
    And a NiFi flow is receiving data from the RemoteProcessGroup named "RemoteProcessGroup" in an input port named "from-minifi" which has the same id as the port named "to_nifi"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And in the "nifi" flow the "success" relationship of the from-minifi node is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "nifi" flow
    And PutFile's failure relationship is auto-terminated in the "nifi" flow

    When NiFi is started
    And all instances start up

    Then in the "nifi" container a single file with the content "test" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds

  Scenario: A NiFi instance produces and transfers data to a MiNiFi instance via s2s using compression
    Given a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "http://nifi-${scenario_id}:8080/nifi"
    And an output port using compression with name "from_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the output port "from_nifi" is connected to the PutFile processor
    And PutFile's success relationship is auto-terminated

    And a NiFi container is set up
    And a directory at "/tmp/input" has a file with the content "test" in the "nifi" flow
    And a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "nifi" flow
    And a NiFi flow is sending data to an output port named "to-minifi-in-nifi" with the id of the port named "from_nifi" from the RemoteProcessGroup named "RemoteProcessGroup"
    And in the "nifi" flow the "success" relationship of the GetFile processor is connected to the to-minifi-in-nifi

    When NiFi is started
    And all instances start up

    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds

  Scenario: A NiFi instance produces and transfers data to a MiNiFi instance via s2s using compression and HTTP protocol
    Given a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "http://nifi-${scenario_id}:8080/nifi" with transport protocol set to "HTTP"
    And an output port using compression with name "from_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the output port "from_nifi" is connected to the PutFile processor
    And PutFile's success relationship is auto-terminated

    And a NiFi container is set up
    And a directory at "/tmp/input" has a file with the content "test" in the "nifi" flow
    And a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "nifi" flow
    And a NiFi flow is sending data to an output port named "to-minifi-in-nifi" with the id of the port named "from_nifi" from the RemoteProcessGroup named "RemoteProcessGroup"
    And in the "nifi" flow the "success" relationship of the GetFile processor is connected to the to-minifi-in-nifi

    When NiFi is started
    And all instances start up

    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 90 seconds
    And the Minifi logs do not contain the following message: "ProcessSession rollback" after 1 seconds
    And the Minifi logs do not contain the following message: "response code 500" after 1 seconds
