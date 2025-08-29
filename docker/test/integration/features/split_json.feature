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
Feature: Splitting JSON content using SplitJson processor
  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: Split multiple query results to separate flow files
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with filename "test_file.json" and content "{"company": {"departments": [{"name": "Engineering", "employees": ["Alice", "Bob"]}, {"name": "Marketing", "employees": "Dave"}, {"name": "Sales", "employees": null}]}}" is present in "/tmp/input"
    And a SplitJson processor with the "JsonPath Expression" property set to "$.company.departments[*].employees"
    And the "Null Value Representation" property of the SplitJson processor is set to "the string 'null'"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And the "success" relationship of the GetFile processor is connected to the SplitJson
    And the "split" relationship of the SplitJson processor is connected to the PutFile
    And the "original" relationship of the SplitJson processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    When the MiNiFi instance starts up
    Then at least one flowfile with the content "["Alice","Bob"]" is placed in the monitored directory in less than 10 seconds
    And at least one flowfile with the content "Dave" is placed in the monitored directory in less than 0 seconds
    And at least one flowfile with the content "null" is placed in the monitored directory in less than 0 seconds
    And at least one flowfile with the content "{"company": {"departments": [{"name": "Engineering", "employees": ["Alice", "Bob"]}, {"name": "Marketing", "employees": "Dave"}, {"name": "Sales", "employees": null}]}}" is placed in the monitored directory in less than 0 seconds
    And the Minifi logs contain the following message: "key:fragment.count value:3" in less than 3 seconds
    And the Minifi logs contain the following message: "key:fragment.index value:0" in less than 0 seconds
    And the Minifi logs contain the following message: "key:fragment.index value:1" in less than 0 seconds
    And the Minifi logs contain the following message: "key:fragment.index value:2" in less than 0 seconds
    And the Minifi logs contain the following message: "key:fragment.identifier value:" in less than 0 seconds
    And the Minifi logs contain the following message: "key:segment.original.filename value:" in less than 0 seconds
