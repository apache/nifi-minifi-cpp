# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

@CORE
Feature: Splitting JSON content using SplitJson processor

  Scenario: Split multiple query results to separate flow files
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file "test_file.json" with the content "{"company": {"departments": [{"name": "Engineering", "employees": ["Alice", "Bob"]}, {"name": "Marketing", "employees": "Dave"}, {"name": "Sales", "employees": null}]}}"
    And a SplitJson processor with the "JsonPath Expression" property set to "$.company.departments[*].employees"
    And the "Null Value Representation" property of the SplitJson processor is set to "the string 'null'"
    And SplitJson is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the SplitJson
    And the "split" relationship of the SplitJson processor is connected to the PutFile
    And the "original" relationship of the SplitJson processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then at least one file with the content "["Alice","Bob"]" is placed in the "/tmp/output" directory in less than 10 seconds
    And at least one file with the content "Dave" is placed in the "/tmp/output" directory in less than 1 seconds
    And at least one file with the content "null" is placed in the "/tmp/output" directory in less than 1 seconds
    And at least one file with the content "{"company": {"departments": [{"name": "Engineering", "employees": ["Alice", "Bob"]}, {"name": "Marketing", "employees": "Dave"}, {"name": "Sales", "employees": null}]}}" is placed in the "/tmp/output" directory in less than 1 seconds
    And the Minifi logs contain the following message: "key:fragment.count value:3" in less than 3 seconds
    And the Minifi logs contain the following message: "key:fragment.index value:0" in less than 1 seconds
    And the Minifi logs contain the following message: "key:fragment.index value:1" in less than 1 seconds
    And the Minifi logs contain the following message: "key:fragment.index value:2" in less than 1 seconds
    And the Minifi logs contain the following message: "key:fragment.identifier value:" in less than 1 seconds
    And the Minifi logs contain the following message: "key:segment.original.filename value:" in less than 1 seconds
