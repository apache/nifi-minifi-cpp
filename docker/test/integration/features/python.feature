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

Feature: MiNiFi can use python processors in its flows
  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance can update attributes through custom python processor
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a ExecutePythonProcessor processor with the "Script File" property set to "/tmp/resources/python/add_attribute_to_flowfile.py"
    And a LogAttribute processor
    And the "success" relationship of the GenerateFlowFile processor is connected to the ExecutePythonProcessor
    And the "success" relationship of the ExecutePythonProcessor processor is connected to the LogAttribute

    When all instances start up
    Then the Minifi logs contain the following message: "key:Python attribute value:attributevalue" in less than 60 seconds

  Scenario: A MiNiFi instance can update attributes through native python processor
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a AddPythonAttribute processor
    And a LogAttribute processor
    And the "success" relationship of the GenerateFlowFile processor is connected to the AddPythonAttribute
    And the "success" relationship of the AddPythonAttribute processor is connected to the LogAttribute

    When all instances start up
    Then the Minifi logs contain the following message: "key:Python attribute value:attributevalue" in less than 60 seconds

  Scenario: Native python processor can read empty input stream
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a MoveContentToJson processor
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GenerateFlowFile processor is connected to the MoveContentToJson
    And the "success" relationship of the MoveContentToJson processor is connected to the PutFile

    When all instances start up
    Then a flowfile with the content '{"content": ""}' is placed in the monitored directory in less than 60 seconds

  Scenario: FlowFile can be removed from session
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a RemoveFlowFile processor

    When all instances start up
    Then the Minifi logs contain the following message: "Removing flow file with UUID" in less than 30 seconds

