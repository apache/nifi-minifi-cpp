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

@ENABLE_PYTHON_SCRIPTING
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

  Scenario: Native python processors can be stateful
    Given a CountingProcessor processor
    And the scheduling period of the CountingProcessor processor is set to "100 ms"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the CountingProcessor processor is connected to the PutFile

    When all instances start up
    Then flowfiles with these contents are placed in the monitored directory in less than 5 seconds: "0,1,2,3,4,5"

  @USE_NIFI_PYTHON_PROCESSORS
  Scenario Outline: MiNiFi C++ can use native NiFi python processors
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with filename "test_file.log" and content "test_data" is present in "/tmp/input"
    And a ParseDocument processor
    And a ChunkDocument processor with the "Chunk Size" property set to "5"
    And the "Chunk Overlap" property of the ChunkDocument processor is set to "3"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And python is installed on the MiNiFi agent <python_install_mode>

    And the "success" relationship of the GetFile processor is connected to the ParseDocument
    And the "success" relationship of the ParseDocument processor is connected to the ChunkDocument
    And the "success" relationship of the ChunkDocument processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute

    When all instances start up
    Then at least one flowfile's content match the following regex: '{"text": "test_", "metadata": {"filename": "test_file.log", "uuid": "", "chunk_index": 0, "chunk_count": 3}}' in less than 30 seconds
    And the Minifi logs contain the following message: "key:document.count value:3" in less than 10 seconds

    Examples: Different python installation modes
      | python_install_mode                                                   |
      | with required python packages                                         |
      | with a pre-created virtualenv                                         |
      | with a pre-created virtualenv containing the required python packages |

  @USE_NIFI_PYTHON_PROCESSORS
  Scenario: MiNiFi C++ can use custom relationships in NiFi native python processors
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with filename "test_file.log" and content "test_data_one" is present in "/tmp/input"
    And a file with filename "test_file2.log" and content "test_data_two" is present in "/tmp/input"
    And a file with filename "test_file3.log" and content "test_data_three" is present in "/tmp/input"
    And a file with filename "test_file4.log" and content "test_data_four" is present in "/tmp/input"
    And a RotatingForwarder processor
    And a PutFile processor with the "Directory" property set to "/tmp/output"

    And the "success" relationship of the GetFile processor is connected to the RotatingForwarder
    And the "first" relationship of the RotatingForwarder processor is connected to the PutFile
    And the "second" relationship of the RotatingForwarder processor is connected to the PutFile
    And the "third" relationship of the RotatingForwarder processor is connected to the PutFile
    And the "fourth" relationship of the RotatingForwarder processor is connected to the PutFile

    When all instances start up

    Then flowfiles with these contents are placed in the monitored directory in less than 10 seconds: "test_data_one,test_data_two,test_data_three,test_data_four"
