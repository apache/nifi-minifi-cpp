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
Feature: Writing JSON path query result to attribute or flow file using EvaluateJsonPath processor

  Scenario: Write query result to flow file
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file ("test_file.json") with the content "{"books": [{"title": "The Great Gatsby", "author": "F. Scott Fitzgerald"}, {"title": "1984", "author": "George Orwell"}]}"
    And a EvaluateJsonPath processor with the "Destination" property set to "flowfile-content"
    And the "JsonPath" property of the EvaluateJsonPath processor is set to "$.books[*].title"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the EvaluateJsonPath
    And the "matched" relationship of the EvaluateJsonPath processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated
    When the MiNiFi instance starts up
    Then a file with the JSON content "["The Great Gatsby","1984"]" is placed in the "/tmp/output" directory in less than 10 seconds

  Scenario: Write query result to attributes
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file ("test_file.json") with the content "{"title": "1984", "author": null}"
    And a EvaluateJsonPath processor with the "Destination" property set to "flowfile-attribute"
    And the "Null Value Representation" property of the EvaluateJsonPath processor is set to "the string 'null'"
    And the "Path Not Found Behavior" property of the EvaluateJsonPath processor is set to "skip"
    And the "title" property of the EvaluateJsonPath processor is set to "$.title"
    And the "author" property of the EvaluateJsonPath processor is set to "$.author"
    And the "release" property of the EvaluateJsonPath processor is set to "$.release"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a LogAttribute processor
    And LogAttribute is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the EvaluateJsonPath
    And the "matched" relationship of the EvaluateJsonPath processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    And PutFile's failure relationship is auto-terminated
    And LogAttribute's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then a file with the JSON content "{"title": "1984", "author": null}" is placed in the "/tmp/output" directory in less than 10 seconds
    And the Minifi logs contain the following message: "key:title value:1984" in less than 10 seconds
    And the Minifi logs contain the following message: "key:author value:null" in less than 1 seconds
    And the Minifi logs do not contain the following message: "key:release" after 10 seconds
