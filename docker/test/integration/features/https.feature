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
Feature: Transfer data from and to MiNiFi using HTTPS

  Background:
    Given the content of "/tmp/output" is monitored


  Scenario: InvokeHTTP to ListenHTTP without an SSLContextService works (no mutual TLS in this case)
    Given a GenerateFlowFile processor with the "Data Format" property set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"
    And the "Custom Text" property of the GenerateFlowFile processor is set to "Lorem ipsum dolor sit amet"
    And a InvokeHTTP processor with the "Remote URL" property set to "https://server-${feature_id}:4430/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And the "success" relationship of the GenerateFlowFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "4430" in a "server" flow
    And the "SSL Certificate" property of the ListenHTTP processor is set to "/tmp/resources/minifi_server.crt"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "server" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then a flowfile with the content "Lorem ipsum dolor sit amet" is placed in the monitored directory in less than 10s


  Scenario: InvokeHTTP to ListenHTTP without an SSLContextService requires a server cert signed by a CA
    Given a GenerateFlowFile processor with the "Data Format" property set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"
    And the "Custom Text" property of the GenerateFlowFile processor is set to "consectetur adipiscing elit"
    And a InvokeHTTP processor with the "Remote URL" property set to "https://server-${feature_id}:4430/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And the "success" relationship of the GenerateFlowFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "4430" in a "server" flow
    And the "SSL Certificate" property of the ListenHTTP processor is set to "/tmp/resources/self_signed_server.crt"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "server" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then no files are placed in the monitored directory in 10s of running time


  Scenario: InvokeHTTP to ListenHTTP with mutual TLS, using certificate files
    Given a GenerateFlowFile processor with the "Data Format" property set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"
    And the "Custom Text" property of the GenerateFlowFile processor is set to "ut labore et dolore magna aliqua"
    And a InvokeHTTP processor with the "Remote URL" property set to "https://server-${feature_id}:4430/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And an ssl context service with a manual CA cert file is set up for InvokeHTTP
    And the "success" relationship of the GenerateFlowFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "4430" in a "server" flow
    And the "SSL Certificate" property of the ListenHTTP processor is set to "/tmp/resources/minifi_server.crt"
    And the "SSL Certificate Authority" property of the ListenHTTP processor is set to "/usr/local/share/certs/ca-root-nss.crt"
    And the "SSL Verify Peer" property of the ListenHTTP processor is set to "yes"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "server" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then a flowfile with the content "ut labore et dolore magna aliqua" is placed in the monitored directory in less than 10s


  Scenario: InvokeHTTP to ListenHTTP without mutual TLS, using the system certificate store
    Given a GenerateFlowFile processor with the "Data Format" property set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"
    And the "Custom Text" property of the GenerateFlowFile processor is set to "Ut enim ad minim veniam"
    And a InvokeHTTP processor with the "Remote URL" property set to "https://server-${feature_id}:4430/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And an ssl context service using the system CA cert store is set up for InvokeHTTP
    And the "success" relationship of the GenerateFlowFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "4430" in a "server" flow
    And the "SSL Certificate" property of the ListenHTTP processor is set to "/tmp/resources/minifi_server.crt"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "server" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then a flowfile with the content "Ut enim ad minim veniam" is placed in the monitored directory in less than 10s


  Scenario: InvokeHTTP to ListenHTTP with mutual TLS, using the system certificate store
    Given a GenerateFlowFile processor with the "Data Format" property set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"
    And the "Custom Text" property of the GenerateFlowFile processor is set to "quis nostrud exercitation ullamco laboris nisi"
    And a InvokeHTTP processor with the "Remote URL" property set to "https://server-${feature_id}:4430/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And an ssl context service using the system CA cert store is set up for InvokeHTTP
    And the "success" relationship of the GenerateFlowFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "4430" in a "server" flow
    And the "SSL Certificate" property of the ListenHTTP processor is set to "/tmp/resources/minifi_server.crt"
    And the "SSL Certificate Authority" property of the ListenHTTP processor is set to "/usr/local/share/certs/ca-root-nss.crt"
    And the "SSL Verify Peer" property of the ListenHTTP processor is set to "yes"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "server" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then a flowfile with the content "quis nostrud exercitation ullamco laboris nisi" is placed in the monitored directory in less than 10s


  Scenario: InvokeHTTP to ListenHTTP without mutual TLS, using the system certificate store, requires a server cert signed by a CA
    Given a GenerateFlowFile processor with the "Data Format" property set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"
    And the "Custom Text" property of the GenerateFlowFile processor is set to "ut aliquip ex ea commodo consequat"
    And a InvokeHTTP processor with the "Remote URL" property set to "https://server-${feature_id}:4430/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And an ssl context service using the system CA cert store is set up for InvokeHTTP
    And the "success" relationship of the GenerateFlowFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "4430" in a "server" flow
    And the "SSL Certificate" property of the ListenHTTP processor is set to "/tmp/resources/self_signed_server.crt"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "server" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then no files are placed in the monitored directory in 10s of running time


  Scenario: InvokeHTTP to ListenHTTP with mutual TLS, using the system certificate store, requires a server cert signed by a CA
    Given a GenerateFlowFile processor with the "Data Format" property set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"
    And the "Custom Text" property of the GenerateFlowFile processor is set to "Duis aute irure dolor in reprehenderit in voluptate"
    And a InvokeHTTP processor with the "Remote URL" property set to "https://server-${feature_id}:4430/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"
    And an ssl context service using the system CA cert store is set up for InvokeHTTP
    And the "success" relationship of the GenerateFlowFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "4430" in a "server" flow
    And the "SSL Certificate" property of the ListenHTTP processor is set to "/tmp/resources/self_signed_server.crt"
    And the "SSL Certificate Authority" property of the ListenHTTP processor is set to "/usr/local/share/certs/ca-root-nss.crt"
    And the "SSL Verify Peer" property of the ListenHTTP processor is set to "yes"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "server" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then no files are placed in the monitored directory in 10s of running time
