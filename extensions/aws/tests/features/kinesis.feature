# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

@ENABLE_AWS
Feature: Sending data from MiNiFi-C++ to an AWS Kinesis server
  In order to transfer data to interact with AWS Kinesis server
  As a user of MiNiFi
  I need to have PutKinesisStream processor

  Scenario: A MiNiFi instance can send data to AWS Kinesis
    Given a kinesis server is set up in correspondence with the PutKinesisStream
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "Schnappi, das kleine Krokodil"
    And a PutKinesisStream processor
    And these processor properties are set
      | processor name     | property name              | property value                           |
      | PutKinesisStream   | Amazon Kinesis Stream Name | test_stream                              |
      | PutKinesisStream   | Access Key                 | test_access_key                          |
      | PutKinesisStream   | Secret Key                 | test_secret                              |
      | PutKinesisStream   | Endpoint Override URL      | http://kinesis-server-${scenario_id}:4568 |
      | PutKinesisStream   | Region                     | us-east-1                                |
    And PutKinesisStream is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the PutKinesisStream
    And the "success" relationship of the PutKinesisStream processor is connected to the PutFile
    And the "failure" relationship of the PutKinesisStream processor is connected to the PutKinesisStream
    And PutFile's success relationship is auto-terminated

    When both instances start up

    Then a single file with the content "Schnappi, das kleine Krokodil" is placed in the "/tmp/output" directory in less than 60 seconds
    And there is a record on the kinesis server with "Schnappi, das kleine Krokodil"
