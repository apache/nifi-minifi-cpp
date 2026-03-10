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

@ENABLE_PROMETHEUS
Feature: MiNiFi can publish metrics to Prometheus server

  Scenario: Published metrics are scraped by Prometheus server
    Given a GetFile processor with the name "GetFile1" and the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile1 processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    And Prometheus is enabled in MiNiFi
    And a Prometheus server is set up
    When all instances start up
    Then "RepositoryMetrics" is published to the Prometheus server in less than 60 seconds
    And "QueueMetrics" is published to the Prometheus server in less than 60 seconds
    And "GetFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "GetFile1" processor
    And "PutFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "PutFile" processor
    And "FlowInformation" is published to the Prometheus server in less than 60 seconds
    And "DeviceInfoNode" is published to the Prometheus server in less than 60 seconds
    And "AgentStatus" is published to the Prometheus server in less than 60 seconds
    And all Prometheus metric types are only defined once

  Scenario: Published metrics are scraped by Prometheus server through SSL connection
    Given a GetFile processor with the name "GetFile1" and the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile1 processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    And Prometheus with SSL is enabled in MiNiFi
    And a Prometheus server is set up with SSL
    When all instances start up
    Then "RepositoryMetrics" is published to the Prometheus server in less than 60 seconds
    And "QueueMetrics" is published to the Prometheus server in less than 60 seconds
    And "GetFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "GetFile1" processor
    And "PutFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "PutFile" processor
    And "FlowInformation" is published to the Prometheus server in less than 60 seconds
    And "DeviceInfoNode" is published to the Prometheus server in less than 60 seconds
    And "AgentStatus" is published to the Prometheus server in less than 60 seconds

  Scenario: Multiple GetFile metrics are reported by Prometheus
    Given a GetFile processor with the name "GetFile1" and the "Input Directory" property set to "/tmp/input"
    And a GetFile processor with the name "GetFile2" and the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile1 processor is set to "true"
    And the "Keep Source File" property of the GetFile2 processor is set to "true"
    And a file with the content "test" is present in "/tmp/input"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile1 processor is connected to the PutFile
    And the "success" relationship of the GetFile2 processor is connected to the PutFile
    And Prometheus is enabled in MiNiFi
    And a Prometheus server is set up
    When all instances start up
    Then "GetFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "GetFile1" processor
    And "GetFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "GetFile2" processor
