# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

@ENABLE_PROMETHEUS
Feature: MiNiFi can publish metrics to Prometheus server

  Scenario: Published metrics are scraped by Prometheus server
    Given a GetFile processor with the name "GetFile1" and the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
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
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

  Scenario: Published metrics are scraped by Prometheus server through SSL connection
    Given a GetFile processor with the name "GetFile1" and the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
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
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

  Scenario: Multiple GetFile metrics are reported by Prometheus
    Given a GetFile processor with the name "GetFile1" and the "Input Directory" property set to "/tmp/input"
    And a GetFile processor with the name "GetFile2" and the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile1 processor is set to "true"
    And the "Keep Source File" property of the GetFile2 processor is set to "true"
    And a directory at "/tmp/input" has a file with the content "test"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile1 processor is connected to the PutFile
    And the "success" relationship of the GetFile2 processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated
    And Prometheus is enabled in MiNiFi
    And a Prometheus server is set up
    When all instances start up
    Then "GetFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "GetFile1" processor
    And "GetFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "GetFile2" processor
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings
