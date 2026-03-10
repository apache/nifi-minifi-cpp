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

@x86_x64_only
@ENABLE_SPLUNK
Feature: Sending data to Splunk HEC using PutSplunkHTTP

  Scenario: A MiNiFi instance transfers data to a Splunk HEC
    Given a Splunk HEC is set up and running
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "foobar"
    And a PutSplunkHTTP processor
    And PutSplunkHTTP is EVENT_DRIVEN
    And a QuerySplunkIndexingStatus processor
    And QuerySplunkIndexingStatus is EVENT_DRIVEN
    And the "Splunk Request Channel" properties of the PutSplunkHTTP and QuerySplunkIndexingStatus processors are set to the same random UUID
    And the "Source" property of the PutSplunkHTTP processor is set to "my-source"
    And the "Source Type" property of the PutSplunkHTTP processor is set to "my-source-type"
    And the "Host" property of the PutSplunkHTTP processor is set to "my-host"
    And the "Hostname" property of the PutSplunkHTTP processor is set to "http://splunk-${scenario_id}"
    And the "Port" property of the PutSplunkHTTP processor is set to "8088"
    And the "Token" property of the PutSplunkHTTP processor is set to "Splunk 176fae97-f59d-4f08-939a-aa6a543f2485"
    And the "Hostname" property of the QuerySplunkIndexingStatus processor is set to "http://splunk-${scenario_id}"
    And the "Port" property of the QuerySplunkIndexingStatus processor is set to "8088"
    And the "Token" property of the QuerySplunkIndexingStatus processor is set to "Splunk 176fae97-f59d-4f08-939a-aa6a543f2485"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN

    And the "success" relationship of the GetFile processor is connected to the PutSplunkHTTP
    And the "success" relationship of the PutSplunkHTTP processor is connected to the QuerySplunkIndexingStatus
    And the "undetermined" relationship of the QuerySplunkIndexingStatus processor is connected to the QuerySplunkIndexingStatus
    And the "acknowledged" relationship of the QuerySplunkIndexingStatus processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When the MiNiFi instance starts up
    Then a single file with the content "foobar" is placed in the "/tmp/output" directory in less than 20 seconds
    And an event is registered in Splunk HEC with the content "foobar" with "my-source" set as source and "my-source-type" set as sourcetype and "my-host" set as host

  Scenario: A MiNiFi instance transfers data to a Splunk HEC with SSL enabled
    Given a Splunk HEC is set up and running
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "foobar"
    And a PutSplunkHTTP processor
    And PutSplunkHTTP is EVENT_DRIVEN
    And a QuerySplunkIndexingStatus processor
    And QuerySplunkIndexingStatus is EVENT_DRIVEN
    And the "Splunk Request Channel" properties of the PutSplunkHTTP and QuerySplunkIndexingStatus processors are set to the same random UUID
    And the "Source" property of the PutSplunkHTTP processor is set to "my-source"
    And the "Source Type" property of the PutSplunkHTTP processor is set to "my-source-type"
    And the "Host" property of the PutSplunkHTTP processor is set to "my-host"
    And the "Hostname" property of the PutSplunkHTTP processor is set to "https://splunk-${scenario_id}"
    And the "Port" property of the PutSplunkHTTP processor is set to "8088"
    And the "Token" property of the PutSplunkHTTP processor is set to "Splunk 176fae97-f59d-4f08-939a-aa6a543f2485"
    And the "Hostname" property of the QuerySplunkIndexingStatus processor is set to "https://splunk-${scenario_id}"
    And the "Port" property of the QuerySplunkIndexingStatus processor is set to "8088"
    And the "Token" property of the QuerySplunkIndexingStatus processor is set to "Splunk 176fae97-f59d-4f08-939a-aa6a543f2485"
    And an ssl context service is set up for PutSplunkHTTP
    And an ssl context service is set up for QuerySplunkIndexingStatus
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN

    And the "success" relationship of the GetFile processor is connected to the PutSplunkHTTP
    And the "success" relationship of the PutSplunkHTTP processor is connected to the QuerySplunkIndexingStatus
    And the "undetermined" relationship of the QuerySplunkIndexingStatus processor is connected to the QuerySplunkIndexingStatus
    And the "acknowledged" relationship of the QuerySplunkIndexingStatus processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    And SSL is enabled for the Splunk HEC and the SSL context service is set up for PutSplunkHTTP and QuerySplunkIndexingStatus

    When the MiNiFi instance starts up
    Then a single file with the content "foobar" is placed in the "/tmp/output" directory in less than 20 seconds
    And an event is registered in Splunk HEC with the content "foobar" with "my-source" set as source and "my-source-type" set as sourcetype and "my-host" set as host
