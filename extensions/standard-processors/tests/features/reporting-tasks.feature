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
Feature: MiNiFi executes first-class reporting tasks defined in the flow config
  In order to observe MiNiFi's behavior from a remote NiFi instance
  As a user of MiNiFi
  I need reporting tasks parsed from the flow config to run and transmit data

  Scenario: SiteToSiteProvenanceReportingTask sends provenance events to NiFi via S2S
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "hello"
    And the "success" relationship of the GetFile processor is auto-terminated
    And MiNiFi configuration "nifi.provenance.repository.class.name" is set to "ProvenanceRepository"
    And a SiteToSiteProvenanceReportingTask reporting task with the name "ProvenanceReporter"

    And a NiFi container is set up
    And a NiFi flow is receiving data on an input port named "from-minifi" from the reporting task named "ProvenanceReporter"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And in the "nifi" flow the "success" relationship of the from-minifi node is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "nifi" flow
    And PutFile's failure relationship is auto-terminated in the "nifi" flow

    When NiFi is started
    And all instances start up

    Then in the "nifi" container at least one file with the content "eventType" is placed in the "/tmp/output" directory in less than 120 seconds
