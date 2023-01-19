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

Feature: Sending data using InvokeHTTP to a receiver using ListenHTTP
  In order to send and receive data via HTTP
  As a user of MiNiFi
  I need to have ListenHTTP and InvokeHTTP processors

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance transfers data to another MiNiFi instance with message body
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a file with the content "test" is present in "/tmp/input"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary:8080/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in a "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then at least one flowfile with the content "test" is placed in the monitored directory in less than 120 seconds

  Scenario: A MiNiFi instance sends data through a HTTP proxy and another one listens
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a file with the content "test" is present in "/tmp/input"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://minifi-listen:8080/contentListener"
    And these processor properties are set to match the http proxy:
      | processor name | property name             | property value |
      | InvokeHTTP     | HTTP Method               | POST           |
      | InvokeHTTP     | Proxy Host                | http-proxy     |
      | InvokeHTTP     | Proxy Port                | 3128           |
      | InvokeHTTP     | invokehttp-proxy-username | admin          |
      | InvokeHTTP     | invokehttp-proxy-password | test101        |
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP

    And a http proxy server is set up accordingly

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in a "minifi-listen" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "minifi-listen" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When all instances start up
    Then at least one flowfile with the content "test" is placed in the monitored directory in less than 120 seconds
    And no errors were generated on the http-proxy regarding "http://minifi-listen:8080/contentListener"

  Scenario: A MiNiFi instance and transfers hashed data to another MiNiFi instance
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a file with the content "test" is present in "/tmp/input"
    And a HashContent processor with the "Hash Attribute" property set to "hash"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary:8080/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And the "success" relationship of the GetFile processor is connected to the HashContent
    And the "success" relationship of the HashContent processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in a "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then at least one flowfile with the content "test" is placed in the monitored directory in less than 120 seconds

  Scenario: A MiNiFi instance transfers data to another MiNiFi instance without message body
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a file with the content "test" is present in "/tmp/input"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary:8080/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And the "Send Message Body" property of the InvokeHTTP processor is set to "false"
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in a "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then at least one empty flowfile is placed in the monitored directory in less than 120 seconds

  Scenario: A MiNiFi instance transfers data to a NiFi instance with message body
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a file with the content "test" is present in "/tmp/input"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://nifi:8081/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP

    And a NiFi flow with the name "nifi" is set up
    And a ListenHTTP processor with the "Listening Port" property set to "8081" in the "nifi" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then at least one flowfile with the content "test" is placed in the monitored directory in less than 120 seconds
