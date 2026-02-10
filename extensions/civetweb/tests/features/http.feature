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

@ENABLE_CIVET
Feature: Sending data using InvokeHTTP to a receiver using ListenHTTP
  In order to send and receive data via HTTP
  As a user of MiNiFi
  I need to have ListenHTTP and InvokeHTTP processors

  Scenario: A MiNiFi instance transfers data to another MiNiFi instance with message body
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a directory at "/tmp/input" has a file with the content "test"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary-${scenario_id}:8080/contentListener"
    And InvokeHTTP is EVENT_DRIVEN
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP
    And InvokeHTTP's success relationship is auto-terminated
    And InvokeHTTP's response relationship is auto-terminated

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in the "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And PutFile is EVENT_DRIVEN in the "secondary" flow
    And in the "secondary" flow the "success" relationship of the ListenHTTP processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "secondary" flow

    When both instances start up
    Then in the "secondary" container at least one file with the content "test" is placed in the "/tmp/output" directory in less than 60 seconds

  Scenario: A MiNiFi instance sends data through a HTTP proxy and another one listens
    Given the http proxy server is set up
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a directory at "/tmp/input" has a file with the content "test"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://minifi-listen-${scenario_id}:8080/contentListener"
    And InvokeHTTP is EVENT_DRIVEN
    And these processor properties are set
      | processor name | property name             | property value                   |
      | InvokeHTTP     | HTTP Method               | POST                             |
      | InvokeHTTP     | Proxy Host                | http-proxy-${scenario_id}        |
      | InvokeHTTP     | Proxy Port                | 3128                             |
      | InvokeHTTP     | invokehttp-proxy-username | admin                            |
      | InvokeHTTP     | invokehttp-proxy-password | test101                          |
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP
    And InvokeHTTP's success relationship is auto-terminated
    And InvokeHTTP's response relationship is auto-terminated

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in the "minifi-listen" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "minifi-listen" flow
    And PutFile is EVENT_DRIVEN in the "minifi-listen" flow
    And in the "minifi-listen" flow the "success" relationship of the ListenHTTP processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "minifi-listen" flow

    When all instances start up
    Then in the "minifi-listen" container at least one file with the content "test" is placed in the "/tmp/output" directory in less than 60 seconds
    And no errors were generated on the http-proxy regarding "http://minifi-listen-${scenario_id}:8080/contentListener"

  Scenario: A MiNiFi instance and transfers hashed data to another MiNiFi instance
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a directory at "/tmp/input" has a file with the content "test"
    And a HashContent processor with the "Hash Attribute" property set to "hash"
    And HashContent is EVENT_DRIVEN
    And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary-${scenario_id}:8080/contentListener"
    And InvokeHTTP is EVENT_DRIVEN
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And the "success" relationship of the GetFile processor is connected to the HashContent
    And the "success" relationship of the HashContent processor is connected to the InvokeHTTP
    And InvokeHTTP's success relationship is auto-terminated
    And InvokeHTTP's response relationship is auto-terminated

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in the "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And PutFile is EVENT_DRIVEN in the "secondary" flow
    And in the "secondary" flow the "success" relationship of the ListenHTTP processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "secondary" flow

    When both instances start up
    Then in the "secondary" container at least one file with the content "test" is placed in the "/tmp/output" directory in less than 60 seconds

  Scenario: A MiNiFi instance transfers data to another MiNiFi instance without message body
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a directory at "/tmp/input" has a file with the content "test"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary-${scenario_id}:8080/contentListener"
    And InvokeHTTP is EVENT_DRIVEN
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And the "Send Message Body" property of the InvokeHTTP processor is set to "false"
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP
    And InvokeHTTP's success relationship is auto-terminated
    And InvokeHTTP's response relationship is auto-terminated

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in the "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And PutFile is EVENT_DRIVEN in the "secondary" flow
    And in the "secondary" flow the "success" relationship of the ListenHTTP processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "secondary" flow

    When both instances start up
    Then in the "secondary" container at least one empty file is placed in the "/tmp/output" directory in less than 60 seconds

  Scenario: A MiNiFi instance transfers data to a NiFi instance with message body
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a directory at "/tmp/input" has a file with the content "test"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://nifi-${scenario_id}:8081/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP

    And a NiFi container is set up
    And a ListenHTTP processor with the "Listening Port" property set to "8081" in the NiFi flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the NiFi flow
    And in the NiFi flow the "success" relationship of the ListenHTTP processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the NiFi flow
    And PutFile's failure relationship is auto-terminated in the NiFi flow

    When both instances start up
    Then in the "nifi" container at least one empty file is placed in the "/tmp/output" directory in less than 60 seconds

  Scenario: A MiNiFi instance transfers data to another MiNiFi instance with message body and limited speed
    Given a GenerateFlowFile processor with the "File Size" property set to "10 MB"
    And the scheduling period of the GenerateFlowFile processor is set to "30 sec"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary-${scenario_id}:8080/contentListener"
    And InvokeHTTP is EVENT_DRIVEN
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And the "Connection Timeout" property of the InvokeHTTP processor is set to "30 s"
    And the "Read Timeout" property of the InvokeHTTP processor is set to "30 s"
    And the "Upload Speed Limit" property of the InvokeHTTP processor is set to "800 KB/s"
    And the "success" relationship of the GenerateFlowFile processor is connected to the InvokeHTTP
    And InvokeHTTP's success relationship is auto-terminated
    And InvokeHTTP's response relationship is auto-terminated

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in the "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And PutFile is EVENT_DRIVEN in the "secondary" flow
    And in the "secondary" flow the "success" relationship of the ListenHTTP processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "secondary" flow

    When both instances start up
    Then in the "secondary" container at least one file with minimum size of "1 MB" is placed in the "/tmp/output" directory in less than 60 seconds
    And the Minifi logs contain the following message: "[warning] InvokeHTTP::onTrigger has been running for" in less than 10 seconds

  Scenario: A MiNiFi instance retrieves data from another MiNiFi instance with message body and limited speed
    Given a InvokeHTTP processor with the "Remote URL" property set to "http://secondary-${scenario_id}:8080/contentListener&testfile"
    And the scheduling period of the InvokeHTTP processor is set to "3 sec"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "GET"
    And the "Connection Timeout" property of the InvokeHTTP processor is set to "30 s"
    And the "Read Timeout" property of the InvokeHTTP processor is set to "30 s"
    And the "Download Speed Limit" property of the InvokeHTTP processor is set to "800 KB/s"
    And the "Data Format" property of the InvokeHTTP processor is set to "Text"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "response" relationship of the InvokeHTTP processor is connected to the PutFile
    And InvokeHTTP's success relationship is auto-terminated
    And InvokeHTTP's response relationship is auto-terminated

    And a GenerateFlowFile processor with the "File Size" property set to "10 MB" in the "secondary" flow
    And a UpdateAttribute processor with the "http.type" property set to "response_body" in the "secondary" flow
    And UpdateAttribute is EVENT_DRIVEN in the "secondary" flow
    And the "filename" property of the UpdateAttribute processor is set to "testfile" in the "secondary" flow
    And the scheduling period of the GenerateFlowFile processor is set to "30 sec" in the "secondary" flow
    And a ListenHTTP processor with the "Listening Port" property set to "8080" in the "secondary" flow
    And ListenHTTP is EVENT_DRIVEN in the "secondary" flow
    And in the "secondary" flow the "success" relationship of the GenerateFlowFile processor is connected to the UpdateAttribute
    And in the "secondary" flow the "success" relationship of the UpdateAttribute processor is connected to the ListenHTTP
    And ListenHTTP's success relationship is auto-terminated in the "secondary" flow

    When both instances start up
    Then at least one file with minimum size of "10 MB" is placed in the "/tmp/output" directory in less than 60 seconds
    And the Minifi logs contain the following message: "[warning] InvokeHTTP::onTrigger has been running for" in less than 10 seconds
