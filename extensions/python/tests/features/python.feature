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
  Scenario: A MiNiFi instance can update attributes through native python processor
    Given the example MiNiFi python processors are present
    And a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a org.apache.nifi.minifi.processors.examples.AddPythonAttribute processor with the name "AddPythonAttribute"
    And AddPythonAttribute is EVENT_DRIVEN
    And a LogAttribute processor
    And LogAttribute is EVENT_DRIVEN
    And the "success" relationship of the GenerateFlowFile processor is connected to the AddPythonAttribute
    And the "success" relationship of the AddPythonAttribute processor is connected to the LogAttribute
    And AddPythonAttribute's success relationship is auto-terminated

    When all instances start up
    Then the Minifi logs contain the following message: "key:Python attribute value:attributevalue" in less than 60 seconds

  Scenario: A MiNiFi instance can handle dynamic properties through native python processor
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a LogDynamicProperties processor with the "Static Property" property set to "static value"
    And LogDynamicProperties is EVENT_DRIVEN
    And the "Dynamic Property" property of the LogDynamicProperties processor is set to "dynamic value"
    And the "success" relationship of the GenerateFlowFile processor is connected to the LogDynamicProperties
    And LogDynamicProperties's success relationship is auto-terminated

    When all instances start up
    Then the Minifi logs contain the following message: "Static Property value: static value" in less than 60 seconds
    And the Minifi logs contain the following message: "Dynamic Property value: dynamic value" in less than 60 seconds
    And the Minifi logs contain the following message: "dynamic property key count: 1" in less than 60 seconds
    And the Minifi logs contain the following message: "dynamic property key: Dynamic Property" in less than 60 seconds

  Scenario: Native python processor can read empty input stream
    Given the example MiNiFi python processors are present
    And a GenerateFlowFile processor with the "File Size" property set to "0B"
    And the scheduling period of the GenerateFlowFile processor is set to "60 sec"
    And a org.apache.nifi.minifi.processors.examples.MoveContentToJson processor with the name "MoveContentToJson"
    And MoveContentToJson is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GenerateFlowFile processor is connected to the MoveContentToJson
    And the "success" relationship of the MoveContentToJson processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up
    Then a single file with the content '{"content": ""}' is placed in the '/tmp/output' directory in less than 60 seconds

  Scenario: FlowFile can be removed from session
    Given the example MiNiFi python processors are present
    And a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a org.apache.nifi.minifi.processors.examples.RemoveFlowFile processor with the name "RemoveFlowFile"
    And RemoveFlowFile is EVENT_DRIVEN
    And the "success" relationship of the GenerateFlowFile processor is connected to the RemoveFlowFile
    And RemoveFlowFile's success relationship is auto-terminated

    When all instances start up
    Then the Minifi logs contain the following message: "Removing flow file with UUID" in less than 30 seconds

  Scenario: Native python processors can be stateful
    Given the example MiNiFi python processors are present
    And a org.apache.nifi.minifi.processors.examples.CountingProcessor processor with the name "CountingProcessor"
    And the scheduling period of the CountingProcessor processor is set to "100 ms"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the CountingProcessor processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up
    Then files with at least these contents "1,2,3,4,5" are placed in the "/tmp/output" directory in less than 5 seconds

  @USE_NIFI_PYTHON_PROCESSORS_WITH_LANGCHAIN
  Scenario Outline: MiNiFi C++ can use native NiFi python processors
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file ("test_file.log") with the content "test_data"
    And a org.apache.nifi.minifi.processors.nifi_python_processors.ParseDocument processor with the name "ParseDocument"
    And ParseDocument is EVENT_DRIVEN
    And a org.apache.nifi.minifi.processors.nifi_python_processors.ChunkDocument processor with the name "ChunkDocument"
    And ChunkDocument is EVENT_DRIVEN
    And the "Chunk Size" property of the ChunkDocument processor is set to "5"
    And the "Chunk Overlap" property of the ChunkDocument processor is set to "3"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And python with langchain is installed on the MiNiFi agent <python_install_mode>

    And the "success" relationship of the GetFile processor is connected to the ParseDocument
    And the "success" relationship of the ParseDocument processor is connected to the ChunkDocument
    And the "success" relationship of the ChunkDocument processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated
    And the MiNiFi deployment timeout is set to 300 seconds

    When all instances start up
    Then at least one file with the content '{"text": "test_", "metadata": {"filename": "test_file.log", "uuid": "", "chunk_index": 0, "chunk_count": 3}}' is placed in the '/tmp/output' directory in less than 30 seconds
    And the Minifi logs contain the following message: "key:document.count value:3" in less than 10 seconds

    Examples: Different python installation modes
      | python_install_mode                                                   |
      | with required python packages                                         |
      | with a pre-created virtualenv                                         |
      | with a pre-created virtualenv containing the required python packages |
      | using inline defined Python dependencies to install packages          |

  Scenario: MiNiFi C++ can use native NiFi source python processors
    Given a org.apache.nifi.minifi.processors.nifi_python_processors.CreateFlowFile processor with the name "CreateFlowFile"
    And the scheduling period of the CreateFlowFile processor is set to "10 seconds"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And the "space" relationship of the CreateFlowFile processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then a single file with the content 'Hello World!' is placed in the '/tmp/output' directory in less than 10 seconds
    And the Minifi logs contain the following message: "key:filename value:" in less than 60 seconds
    And the Minifi logs contain the following message: "key:type value:space" in less than 60 seconds

  Scenario: MiNiFi C++ can use custom relationships in NiFi native python processors
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file ("test_file.log") with the content "test_data_one"
    And a directory at "/tmp/input" has a file ("test_file2.log") with the content "test_data_two"
    And a directory at "/tmp/input" has a file ("test_file3.log") with the content "test_data_three"
    And a directory at "/tmp/input" has a file ("test_file4.log") with the content "test_data_four"
    And a org.apache.nifi.minifi.processors.nifi_python_processors.RotatingForwarder processor with the name "RotatingForwarder"
    And RotatingForwarder is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"

    And the "success" relationship of the GetFile processor is connected to the RotatingForwarder
    And the "first" relationship of the RotatingForwarder processor is connected to the PutFile
    And the "second" relationship of the RotatingForwarder processor is connected to the PutFile
    And the "third" relationship of the RotatingForwarder processor is connected to the PutFile
    And the "fourth" relationship of the RotatingForwarder processor is connected to the PutFile

    When all instances start up

    Then files with contents "test_data_one,test_data_two,test_data_three,test_data_four" are placed in the "/tmp/output" directory in less than 10 seconds

  Scenario: MiNiFi C++ can use special property types including controller services in NiFi native python processors
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And the scheduling period of the GenerateFlowFile processor is set to "30 sec"
    And a org.apache.nifi.minifi.processors.nifi_python_processors.SpecialPropertyTypeChecker processor with the name "SpecialPropertyTypeChecker"
    And SpecialPropertyTypeChecker is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And an ssl context service is set up for SpecialPropertyTypeChecker

    And the "success" relationship of the GenerateFlowFile processor is connected to the SpecialPropertyTypeChecker
    And the "success" relationship of the SpecialPropertyTypeChecker processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up

    Then a single file with the content 'Check successful!' is placed in the '/tmp/output' directory in less than 30 seconds

  Scenario: NiFi native python processor's ProcessContext interface can be used in MiNiFi C++
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And the scheduling period of the GenerateFlowFile processor is set to "30 sec"
    And a org.apache.nifi.minifi.processors.nifi_python_processors.ProcessContextInterfaceChecker processor with the name "ProcessContextInterfaceChecker"
    And ProcessContextInterfaceChecker is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN

    And the "success" relationship of the GenerateFlowFile processor is connected to the ProcessContextInterfaceChecker
    And the "myrelationship" relationship of the ProcessContextInterfaceChecker processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up

    Then a single file with the content 'Check successful!' is placed in the '/tmp/output' directory in less than 30 seconds

  Scenario: NiFi native python processor can update attributes of a flow file transferred to failure relationship
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a UpdateAttribute processor with the "my.attribute" property set to "my.value"
    And UpdateAttribute is EVENT_DRIVEN
    And the "error.message" property of the UpdateAttribute processor is set to "Old error"
    And a org.apache.nifi.minifi.processors.nifi_python_processors.FailureWithAttributes processor with the name "FailureWithAttributes"
    And FailureWithAttributes is EVENT_DRIVEN
    And a LogAttribute processor
    And LogAttribute is EVENT_DRIVEN

    And the "success" relationship of the GenerateFlowFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the FailureWithAttributes
    And the "failure" relationship of the FailureWithAttributes processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated

    When all instances start up

    Then the Minifi logs contain the following message: "key:error.message value:Error" in less than 60 seconds
    And the Minifi logs contain the following message: "key:my.attribute value:my.value" in less than 10 seconds

  Scenario: NiFi native python processors support relative imports
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And the scheduling period of the GenerateFlowFile processor is set to "30 sec"
    And a org.apache.nifi.minifi.processors.nifi_python_processors.compute.processors.RelativeImporterProcessor processor with the name "RelativeImporterProcessor"
    And RelativeImporterProcessor is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN

    And the "success" relationship of the GenerateFlowFile processor is connected to the RelativeImporterProcessor
    And the "success" relationship of the RelativeImporterProcessor processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up

    Then a single file with the content 'The final result is 1990' is placed in the '/tmp/output' directory in less than 30 seconds

  Scenario: NiFi native python processor is allowed to be triggered without creating any flow files
    Given a org.apache.nifi.minifi.processors.nifi_python_processors.CreateNothing processor with the name "CreateNothing"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the CreateNothing processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then no files are placed in the "/tmp/output" directory in 10 seconds of running time
    And the Minifi logs do not contain the following message: "Caught Exception during SchedulingAgent::onTrigger of processor CreateNothing" after 1 seconds

  Scenario: NiFi native python processor cannot specify content of failure result
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a org.apache.nifi.minifi.processors.nifi_python_processors.FailureWithContent processor with the name "FailureWithContent"
    And FailureWithContent is EVENT_DRIVEN

    And the "success" relationship of the GenerateFlowFile processor is connected to the FailureWithContent

    When all instances start up

    Then the Minifi logs contain the following message: "'failure' relationship should not have content, the original flow file will be transferred automatically in this case." in less than 60 seconds

  Scenario: NiFi native python processor cannot transfer to original relationship
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a org.apache.nifi.minifi.processors.nifi_python_processors.TransferToOriginal processor with the name "TransferToOriginal"
    And TransferToOriginal is EVENT_DRIVEN

    And the "success" relationship of the GenerateFlowFile processor is connected to the TransferToOriginal

    When all instances start up

    Then the Minifi logs contain the following message: "Result relationship cannot be 'original', it is reserved for the original flow file, and transferred automatically in non-failure cases." in less than 60 seconds

  Scenario: MiNiFi C++ supports RecordTransform native python processors
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at '/tmp/input' has a file with the content '{"group": "group1", "name": "John"}\n{"group": "group1", "name": "Jane"}\n{"group": "group2", "name": "Kyle"}\n{"name": "Zoe"}'
    And a directory at '/tmp/input' has a file with the content '{"group": "group1", "name": "Steve"}\n{}'
    And a org.apache.nifi.minifi.processors.nifi_python_processors.SetRecordField processor with the name "SetRecordField"
    And SetRecordField is EVENT_DRIVEN
    And the "Record Reader" property of the SetRecordField processor is set to "JsonTreeReader"
    And the "Record Writer" property of the SetRecordField processor is set to "JsonRecordSetWriter"
    And a JsonTreeReader controller service is set up
    And a JsonRecordSetWriter controller service is set up and the "Output Grouping" property set to "Array"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And the "Log Payload" property of the LogAttribute processor is set to "true"

    And the "success" relationship of the GetFile processor is connected to the SetRecordField
    And the "success" relationship of the SetRecordField processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated

    When all instances start up

    Then the Minifi logs contain the following message: '[{"group":"group1","name":"John"},{"group":"group1","name":"Jane"}]' in less than 60 seconds
    And the Minifi logs contain the following message: '[{"group":"group2","name":"Kyle"}]' in less than 5 seconds
    And the Minifi logs contain the following message: '[{"name":"Zoe"}]' in less than 5 seconds
    And the Minifi logs contain the following message: '[{"group":"group1","name":"Steve"}]' in less than 5 seconds
    And the Minifi logs contain the following message: '[{}]' in less than 5 seconds

  Scenario: MiNiFi C++ can use state manager commands in native NiFi python processors
    Given a org.apache.nifi.minifi.processors.nifi_python_processors.TestStateManager processor with the name "TestStateManager"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And the "success" relationship of the TestStateManager processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "key:state_key value:1" in less than 10 seconds
    And the Minifi logs contain the following message: "key:state_key value:2" in less than 10 seconds

  Scenario: MiNiFi C++ can use dynamic properties in native NiFi python processors
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a org.apache.nifi.minifi.processors.nifi_python_processors.NifiStyleLogDynamicProperties processor with the name "NifiStyleLogDynamicProperties"
    And NifiStyleLogDynamicProperties is EVENT_DRIVEN
    And the "Static Property" property of the NifiStyleLogDynamicProperties processor is set to "static value"
    And the "Dynamic Property" property of the NifiStyleLogDynamicProperties processor is set to "dynamic value"
    And the "success" relationship of the GenerateFlowFile processor is connected to the NifiStyleLogDynamicProperties
    And NifiStyleLogDynamicProperties's success relationship is auto-terminated

    When all instances start up
    Then the Minifi logs contain the following message: "Static Property value: static value" in less than 10 seconds
    And the Minifi logs contain the following message: "Dynamic Property value: dynamic value" in less than 10 seconds

  Scenario: MiNiFi C++ allows NiFi python processors to use property validators for expression language enabled properties
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a org.apache.nifi.minifi.processors.nifi_python_processors.ExpressionLanguagePropertyWithValidator processor with the name "ExpressionLanguagePropertyWithValidator"
    And ExpressionLanguagePropertyWithValidator is EVENT_DRIVEN
    And the "Integer Property" property of the ExpressionLanguagePropertyWithValidator processor is set to "42"
    And the "success" relationship of the GenerateFlowFile processor is connected to the ExpressionLanguagePropertyWithValidator
    And ExpressionLanguagePropertyWithValidator's success relationship is auto-terminated

    When all instances start up
    Then the Minifi logs contain the following message: "Integer Property value: 42" in less than 10 seconds

  Scenario: MiNiFi C++ rolls back session if expression language cannot be evaluated as integer in NiFi python processors
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a UpdateAttribute processor with the "my.integer" property set to "invalid"
    And UpdateAttribute is EVENT_DRIVEN
    And a org.apache.nifi.minifi.processors.nifi_python_processors.ExpressionLanguagePropertyWithValidator processor with the name "ExpressionLanguagePropertyWithValidator"
    And ExpressionLanguagePropertyWithValidator is EVENT_DRIVEN
    And the "Integer Property" property of the ExpressionLanguagePropertyWithValidator processor is set to "${my.integer}"
    And the "success" relationship of the GenerateFlowFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the ExpressionLanguagePropertyWithValidator
    And ExpressionLanguagePropertyWithValidator's success relationship is auto-terminated

    When all instances start up
    Then the Minifi logs contain the following message: "ProcessSession rollback for ExpressionLanguagePropertyWithValidator" in less than 10 seconds

  Scenario: MiNiFi C++ can use evaluate expression language expressions correctly using the NiFi python API
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And the scheduling period of the GenerateFlowFile processor is set to "30 seconds"
    And a UpdateAttribute processor with the "my.attribute" property set to "my.value"
    And UpdateAttribute is EVENT_DRIVEN
    And a org.apache.nifi.minifi.processors.nifi_python_processors.EvaluateExpressionLanguageChecker processor with the name "EvaluateExpressionLanguageChecker"
    And EvaluateExpressionLanguageChecker is EVENT_DRIVEN
    And the "EL Property" property of the EvaluateExpressionLanguageChecker processor is set to "${my.attribute:toUpper()}"
    And the "Non EL Property" property of the EvaluateExpressionLanguageChecker processor is set to "non el ${my.attribute}"
    And the "My Dynamic Property" property of the EvaluateExpressionLanguageChecker processor is set to "Dynamic ${my.attribute}"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN

    And the "success" relationship of the GenerateFlowFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the EvaluateExpressionLanguageChecker
    And the "success" relationship of the EvaluateExpressionLanguageChecker processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up

    Then a single file with the content 'Check successful!' is placed in the '/tmp/output' directory in less than 30 seconds
    And the Minifi logs contain the following message: "EL Property value: ${my.attribute:toUpper()}" in less than 1 seconds
    And the Minifi logs contain the following message: "Evaluated EL Property value: MY.VALUE" in less than 1 seconds
    And the Minifi logs contain the following message: "Non EL Property value: non el ${my.attribute}" in less than 1 seconds
    And the Minifi logs contain the following message: "Evaluated Non EL Property value: non el ${my.attribute}" in less than 1 seconds
    And the Minifi logs contain the following message: "Non-existent property value is empty" in less than 1 seconds
    And the Minifi logs contain the following message: "My Dynamic Property value is: Dynamic ${my.attribute}" in less than 1 seconds
    And the Minifi logs contain the following message: "My Dynamic Property evaluated value is: Dynamic my.value" in less than 1 seconds
